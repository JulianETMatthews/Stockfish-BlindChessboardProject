


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
