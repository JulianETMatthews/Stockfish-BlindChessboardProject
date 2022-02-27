/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2021 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <vector>

#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "timeman.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"


using namespace std;

extern vector<string> setup_bench(const Position&, istream&);

namespace {

  // FEN string of the initial position, normal chess
  const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";


  // position() is called when engine receives the "position" UCI command.
  // The function sets up the position described in the given FEN string ("fen")
  // or the starting position ("startpos") and then makes the moves given in the
  // following move list ("moves").

  void position(Position& pos, istringstream& is, StateListPtr& states) {

    Move m;
    string token, fen;

    is >> token;

    if (token == "startpos")
    {
        fen = StartFEN;
        is >> token; // Consume "moves" token if any
    }
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
    pos.set(fen, Options["UCI_Chess960"], &states->back(), Threads.main());

    // Parse move list (if any)
    while (is >> token && (m = UCI::to_move(pos, token)) != MOVE_NONE)
    {
        states->emplace_back();
        pos.do_move(m, states->back());
    }
  }

  // trace_eval() prints the evaluation for the current position, consistent with the UCI
  // options set so far.

/* NOT USED:
  void trace_eval(Position& pos) {

    StateListPtr states(new std::deque<StateInfo>(1));
    Position p;
    p.set(pos.fen(), Options["UCI_Chess960"], &states->back(), Threads.main());

    Eval::NNUE::verify();

    sync_cout << "\n" << Eval::trace(p) << sync_endl;
  }


  // setoption() is called when engine receives the "setoption" UCI command. The
  // function updates the UCI option ("name") to the given value ("value").


  void setoption(istringstream& is) {

    string token, name, value;

    is >> token; // Consume "name" token

    // Read option name (can contain spaces)
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;

    // Read option value (can contain spaces)
    while (is >> token)
        value += (value.empty() ? "" : " ") + token;

    if (Options.count(name)){
        Options[name] = value;
        std::cout<<Options[name]<<std::endl;
    }else{
        sync_cout << "No such option: " << name << sync_endl;
      }
  }
*/

  // go() is called when engine receives the "go" UCI command. The function sets
  // the thinking time and other parameters from the input string, then starts
  // the search.

  void go(Position& pos, istringstream& is, StateListPtr& states) {

    Search::LimitsType limits;
    string token;
    bool ponderMode = false;

    limits.startTime = now(); // As early as possible!

    while (is >> token)
        if (token == "searchmoves") // Needs to be the last command on the line
            while (is >> token)
                limits.searchmoves.push_back(UCI::to_move(pos, token));

        else if (token == "wtime")     is >> limits.time[WHITE];
        else if (token == "btime")     is >> limits.time[BLACK];
        else if (token == "winc")      is >> limits.inc[WHITE];
        else if (token == "binc")      is >> limits.inc[BLACK];
        else if (token == "movestogo") is >> limits.movestogo;
        else if (token == "depth")     is >> limits.depth;
        else if (token == "nodes")     is >> limits.nodes;
        else if (token == "movetime")  is >> limits.movetime;
        else if (token == "mate")      is >> limits.mate;
        else if (token == "perft")     is >> limits.perft;
        else if (token == "infinite")  limits.infinite = 1;
        else if (token == "ponder")    ponderMode = true;

    Threads.start_thinking(pos, states, limits, ponderMode);
    //std::cout<<pos<<std::endl;
  }


  // bench() is called when engine receives the "bench" command. Firstly
  // a list of UCI commands is setup according to bench parameters, then
  // it is run one by one printing a summary at the end.

/* NOT USED:
  void bench(Position& pos, istream& args, StateListPtr& states) {

    string token;
    uint64_t num, nodes = 0, cnt = 1;

    vector<string> list = setup_bench(pos, args);
    num = count_if(list.begin(), list.end(), [](string s) { return s.find("go ") == 0 || s.find("eval") == 0; });

    TimePoint elapsed = now();

    for (const auto& cmd : list)
    {
        istringstream is(cmd);
        is >> skipws >> token;

        if (token == "go" || token == "eval")
        {
            cerr << "\nPosition: " << cnt++ << '/' << num << " (" << pos.fen() << ")" << endl;
            if (token == "go")
            {
               go(pos, is, states);
               Threads.main()->wait_for_search_finished();
               nodes += Threads.nodes_searched();
            }
            else
               trace_eval(pos);
        }
        else if (token == "setoption")  setoption(is);
        else if (token == "position")   position(pos, is, states);
        else if (token == "ucinewgame") { Search::clear(); elapsed = now(); } // Search::clear() may take some while
    }

    elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

    dbg_print(); // Just before exiting

    cerr << "\n==========================="
         << "\nTotal time (ms) : " << elapsed
         << "\nNodes searched  : " << nodes
         << "\nNodes/second    : " << 1000 * nodes / elapsed << endl;
  }
*/


  // The win rate model returns the probability (per mille) of winning given an eval
  // and a game-ply. The model fits rather accurately the LTC fishtest statistics.
  int win_rate_model(Value v, int ply) {

     // The model captures only up to 240 plies, so limit input (and rescale)
     double m = std::min(240, ply) / 64.0;

     // Coefficients of a 3rd order polynomial fit based on fishtest data
     // for two parameters needed to transform eval to the argument of a
     // logistic function.
     double as[] = {-8.24404295, 64.23892342, -95.73056462, 153.86478679};
     double bs[] = {-3.37154371, 28.44489198, -56.67657741,  72.05858751};
     double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
     double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

     // Transform eval to centipawns with limited range
     double x = std::clamp(double(100 * v) / PawnValueEg, -1000.0, 1000.0);

     // Return win rate in per mille (rounded to nearest)
     return int(0.5 + 1000 / (1 + std::exp((a - x) / b)));
  }

} // namespace



//                Custom written and modified for RPI IED Spring 2021 Group 1 - Blind Chessboard
//              |--------------------------------------------------------------------------------|

int moveToNumber(string themove){
  //Converts a move such as "e2e4" to a number from 0 to 63 corresponding 
  //to the 64 squares ordered bottom left to top right
  string secondsquare = themove.substr(2, 2);
  int index = toupper(secondsquare[0]) - 'A';
  return index + 8 * (stoi(secondsquare.substr(1,1))-1);
}

string printPieceRaising(string thesquare){
  // Prints visualization of the single square that is raised on the chessboard
  string output;
  std::string s = "+---+---+---+---+---+---+---+---+\n";
  vector<string> ranks{"8", "7", "6", "5", "4", "3", "2", "1"};
  vector<string> files{"a","b","c","d","e","f","g","h"};
  int i = 8;
  for (vector<string>::iterator it = ranks.begin(); it != ranks.end(); it++){
    output.append(s);
    
    for (vector<string>::iterator itr = files.begin(); itr != files.end(); itr++){
      output.append("| ");
      if (*it == thesquare.substr(1, 1) && *itr == thesquare.substr(0, 1)){
        output.append("#");
      }else{
        output.append(" ");
      }
      output.append(" ");
    }
    output.append("|");
    output.append("  ");
    output.append(to_string(i));
    output.append("\n");
    i--;
  }
  output.append(s);
  output.append("  a   b   c   d   e   f   g   h\n");
  return output;
}


string decToBinary(int n){
  // Converts decimal to binary
    // array to store binary number
    int binaryNum[32];
 
    // counter for binary array
    int i = 0;
    while (n > 0) {
 
        // storing remainder in binary array
        binaryNum[i] = n % 2;
        n = n / 2;
        i++;
    }
 
    // printing binary array in reverse order
    string output; 
    for (int j = i - 1; j >= 0; j--)
        output.append(to_string(binaryNum[j]));
    return output;
}

void removeLastMove(string &PGN, string &PGN_command){
  // Removes the last move from the PGN string
  PGN = PGN.substr(0, PGN.size()-5);
  PGN_command = PGN_command.substr(0, PGN_command.size()-5);
}

string getLastMove(string PGN){
  // Takes the substring of the last move from the PGN string
  string themove = PGN.substr(PGN.size()-5, 4);
  return themove;
}

string getLastMovedSquare(string PGN){
  // Takes the substring that is just the square the last piece was moved to 
  string themove = PGN.substr(PGN.size()-5, 4);
  return themove.substr(2, 2);
}

void pieceRaisingOutput(string PGN, vector<string> PGN_vec){
  // Outputs all piece raising information
  if (PGN.size() == 0){
            std::cout<<"No pieces raised"<<std::endl;
          }else{
            std::cout<<"\nLast Move:\t\t"<<PGN_vec.back()<<std::endl;
            std::cout<<"Piece to raise:\t\t"<<getLastMovedSquare(PGN)<<std::endl;
            int square = moveToNumber(getLastMove(PGN));
            std::cout<<"Decimal Equivalent\t"<<square<<std::endl;
            string square_binary = decToBinary(square);
            std::cout<<"Binary Output:\t\t"<<square_binary<<std::endl;
            std::cout<<printPieceRaising(PGN_vec.back().substr(2, 2));
          }
}

void UCI::ChessboardLoop(int argc, char* argv[]){
  // Waits for a command from stdin, parses it and does the appropriate operation. 
  // Modified from UCI::loop in original code. 

  // Object Setup
  Position pos;
  string token, cmd;
  string PGN;
  vector<string> PGN_vec;
  string PGN_command = "startpos moves ";
  StateListPtr states(new std::deque<StateInfo>(1));

  pos.set(StartFEN, false, &states->back(), Threads.main());

  // Parsing
  for (int i = 1; i < argc; ++i){
      cmd += std::string(argv[i]) + " ";
    }

  do {
    // Waiting for input
    if (argc == 1 && !getline(cin, cmd)){
            cmd = "quit";
      }

        istringstream is(cmd);

        token.clear(); 
        // Avoid a stale if getline() returns empty or blank line
        is >> skipws >> token;

// Operations based on inputs:
        if (token == "quit" || token == "stop"){
          // Quit
          Threads.stop = true;

        }else if (token == "printposition"){
          // Prints current position
          istringstream is_2(PGN_command);
          position(pos, is_2, states);
          std::cout<<pos<<std::endl;

        }else if (token == "move"){
          // Records input move, checks legality of move
          string themove;
          is>>skipws>>themove;
          if (themove.size() != 4){
            std::cout<<"Invalid move format"<<std::endl;
          }else{
            PGN.append(themove);
            PGN.append(" ");
            PGN_command.append(themove);
            PGN_command.append(" ");
            

            int PGN_vec_size = PGN_vec.size();
            if (    move(to_move(pos, themove), false) != "(none)" &&
                    move(to_move(pos, themove), false) != "0000"){
            // Checks move legality
              PGN_vec.push_back(move(to_move(pos, themove), false));
            }

            if (PGN_vec_size == int(PGN_vec.size())){
              // The move is not a legal move
              std::cout<<"Not a legal move"<<std::endl;
              removeLastMove(PGN, PGN_command);
            }else{
              // The move is a legal move
              istringstream is_2(PGN_command);
              position(pos, is_2, states);
              std::cout<<pos<<std::endl;

              std::cout<<"PGN vector: ";
              for (int i = 0; i < int(PGN_vec.size()); i++){
                std::cout<<to_string(i+1)<<". "<<PGN_vec[i];
                if (i != int(PGN_vec.size())-1)
                  std::cout<<" ";
              }
              std::cout<<"\n\n";
              // Piece raising output visualization
              pieceRaisingOutput(PGN, PGN_vec);
            }
          }
          
        }else if (token == "bestmove"){
          // Outputs depth 22 best computer move
          istringstream is_2("go depth 22");
          go(pos, is_2, states);

        }else if (token == "removelastmove"){
          // Removes last move
          if (PGN.size() != 0){
            removeLastMove(PGN, PGN_command);
          }
          if (PGN_vec.size() > 0){
            PGN_vec.pop_back();
          }

        }else if (token == "getpieceraise"){
          // Piece raising output visualization
          pieceRaisingOutput(PGN, PGN_vec);

        }else if (token == "getPGN"){
          std::cout<<"PGN vector: ";
              for (int i = 0; i < int(PGN_vec.size()); i++){
                std::cout<<to_string(i+1)<<". "<<PGN_vec[i];
                if (i != int(PGN_vec.size())-1)
                  std::cout<<" ";
              }
              std::cout<<"\n\n";
        }else
          sync_cout << "Unknown command: " << cmd << sync_endl<<std::endl;
        std::cout<<"\n*****************************************\n\n";


  } while (token != "quit" && argc == 1);


}

//              |-------------------------------------------------------------------|


/*
void UCI::loop(int argc, char* argv[]) {

  Position pos;
  string token, cmd;
  StateListPtr states(new std::deque<StateInfo>(1));

  pos.set(StartFEN, false, &states->back(), Threads.main());

  for (int i = 1; i < argc; ++i)
      cmd += std::string(argv[i]) + " ";

  do {
      if (argc == 1 && !getline(cin, cmd)) // Block here waiting for input or EOF
          cmd = "quit";

      istringstream is(cmd);

      token.clear(); // Avoid a stale if getline() returns empty or blank line
      is >> skipws >> token;

      
      if (    token == "quit"
          ||  token == "stop")
          Threads.stop = true;

      // The GUI sends 'ponderhit' to tell us the user has played the expected move.
      // So 'ponderhit' will be sent if we were told to ponder on the same move the
      // user has played. We should continue searching but switch from pondering to
      // normal search.
      else if (token == "ponderhit")
          Threads.main()->ponder = false; // Switch to normal search

      else if (token == "uci")
          sync_cout << "id name " << engine_info(true)
                    << "\n"       << Options
                    << "\nuciok"  << sync_endl;

      else if (token == "setoption")  setoption(is);
      else if (token == "go")         go(pos, is, states);
      else if (token == "position")   position(pos, is, states);
      else if (token == "ucinewgame") Search::clear();
      else if (token == "isready")    sync_cout << "readyok" << sync_endl;

      // Additional custom non-UCI commands, mainly for debugging.
      // Do not use these commands during a search!
      else if (token == "flip")     pos.flip();
      else if (token == "bench")    bench(pos, is, states);
      else if (token == "d")        sync_cout << pos << sync_endl;
      else if (token == "eval")     trace_eval(pos);
      else if (token == "compiler") sync_cout << compiler_info() << sync_endl;
      else
          sync_cout << "Unknown command: " << cmd << sync_endl;
        std::cout<<pos<<std::endl;
  } while (token != "quit" && argc == 1); // Command line args are one-shot
}
*/

/// UCI::value() converts a Value to a string suitable for use with the UCI
/// protocol specification:
///
/// cp <x>    The score from the engine's point of view in centipawns.
/// mate <y>  Mate in y moves, not plies. If the engine is getting mated
///           use negative values for y.

string UCI::value(Value v) {

  assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);

  stringstream ss;

  if (abs(v) < VALUE_MATE_IN_MAX_PLY)
      ss << "cp " << v * 100 / PawnValueEg;
  else
      ss << "mate " << (v > 0 ? VALUE_MATE - v + 1 : -VALUE_MATE - v) / 2;

  return ss.str();
}


/// UCI::wdl() report WDL statistics given an evaluation and a game ply, based on
/// data gathered for fishtest LTC games.

string UCI::wdl(Value v, int ply) {

  stringstream ss;

  int wdl_w = win_rate_model( v, ply);
  int wdl_l = win_rate_model(-v, ply);
  int wdl_d = 1000 - wdl_w - wdl_l;
  ss << " wdl " << wdl_w << " " << wdl_d << " " << wdl_l;

  return ss.str();
}


/// UCI::square() converts a Square to a string in algebraic notation (g1, a7, etc.)

std::string UCI::square(Square s) {
  return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
}


/// UCI::move() converts a Move to a string in coordinate notation (g1f3, a7a8q).
/// The only special case is castling, where we print in the e1g1 notation in
/// normal chess mode, and in e1h1 notation in chess960 mode. Internally all
/// castling moves are always encoded as 'king captures rook'.

string UCI::move(Move m, bool chess960) {

  Square from = from_sq(m);
  Square to = to_sq(m);

  if (m == MOVE_NONE)
      return "(none)";

  if (m == MOVE_NULL)
      return "0000";

  if (type_of(m) == CASTLING && !chess960)
      to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

  string move = UCI::square(from) + UCI::square(to);

  if (type_of(m) == PROMOTION)
      move += " pnbrqk"[promotion_type(m)];

  return move;
}


/// UCI::to_move() converts a string representing a move in coordinate notation
/// (g1f3, a7a8q) to the corresponding legal Move, if any.

Move UCI::to_move(const Position& pos, string& str) {

  if (str.length() == 5) // Junior could send promotion piece in uppercase
      str[4] = char(tolower(str[4]));

  for (const auto& m : MoveList<LEGAL>(pos))
      if (str == UCI::move(m, pos.is_chess960()))
          return m;

  return MOVE_NONE;
}
