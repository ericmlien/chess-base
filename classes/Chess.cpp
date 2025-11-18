#include "Chess.h"
#include <limits>
#include <cmath>

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    for (int sq = 0; sq < 64; sq++) {
        _knightBitBoards[sq] = generateKnightMoveBitBoard(sq);
        _kingBitBoards[sq] = generateKingMoveBitBoard(sq);
    }
    _moves = generateAllMoves();
    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)
    _grid -> forEachSquare([](ChessSquare* square, int x, int y) {
        square -> setBit(nullptr);
    });

    int row = 7;
    int col = 0;

    for (char ch : fen) {
        if (ch == '/') {
            row--;
            col = 0;
        } else if (isdigit(ch)) {
            col += ch - '0';
        } else {
            ChessPiece piece = Pawn;
            switch (toupper(ch)) {
                case 'P':
                    piece = Pawn;
                    break;
                case 'N':
                    piece = Knight;
                    break;
                case 'B':
                    piece = Bishop;
                    break;
                case 'R':
                    piece = Rook;
                    break;
                case 'Q':
                    piece = Queen;
                    break;
                case 'K':
                    piece = King;
                    break;
            }
            Bit* bit = PieceForPlayer(isupper(ch) ? 0 : 1, piece);
            ChessSquare *square = _grid -> getSquare(col, row);
            bit -> setPosition(square -> getPosition());
            bit -> setParent(square);
            bit -> setGameTag(isupper(ch) ? piece : piece + 128);
            square -> setBit(bit);
            col++;
        }
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

std::vector<BitMove> Chess::generateAllMoves() {

    std::string state = stateString();
    std::vector<BitMove> moves;
    moves.reserve(32);

    uint64_t whitePawns = 0ULL;
    uint64_t whiteKnights = 0ULL;
    uint64_t whiteKing = 0ULL;

    uint64_t blackPawns = 0ULL;
    uint64_t blackKnights = 0ULL;
    uint64_t blackKing = 0ULL;

    uint64_t whiteOccupancy = 0ULL;
    uint64_t blackOccupancy = 0ULL;

    for (int i = 0; i < 64; i++) {


        //Add each piece to its team's occupancy board
        if (state[i] != '0') {
            //If the piece is labeled as upper-case, then it belongs to the white team.
            //If not, it belongs to the black team.
            //We'll use this method to check for teams everywhere else!
            if (state[i] == toupper(state[i])) {
                whiteOccupancy |= 1ULL << i;
            } else {
                blackOccupancy |= 1ULL << i;
            }
        }

        switch (toupper(state[i])) {
            case 'P':
                if (state[i] == toupper(state[i])) {
                    whitePawns |= 1ULL << i;
                } else {
                    blackPawns |= 1ULL << i;
                }
                break;
            case 'N':
                if (state[i] == toupper(state[i])) {
                    whiteKnights |= 1ULL << i;
                } else {
                    blackKnights |= 1ULL << i;
                }
                break;
            case 'K':
                if (state[i] == toupper(state[i])) {
                    whiteKing |= 1ULL << i;
                } else {
                    blackKing |= 1ULL << i;
                }
                break;
        }
    }

    uint64_t occupancy = whiteOccupancy | blackOccupancy;

    generatePawnMoves(moves, whitePawns, ~occupancy, blackOccupancy, WHITE);
    generateKnightMoves(moves, whiteKnights, ~whiteOccupancy);
    generateKingMoves(moves, whiteKing, ~whiteOccupancy);

    generatePawnMoves(moves, blackPawns, ~occupancy, whiteOccupancy, BLACK);
    generateKnightMoves(moves, blackKnights, ~blackOccupancy);
    generateKingMoves(moves, blackKing, ~blackOccupancy);

    return moves;
}

#pragma region pawnMoves

void Chess::generatePawnMoves(std::vector<BitMove>& moves, const BitBoard pawns, const BitBoard emptySquares, const BitBoard enemyPieces, char color) {
    if (pawns.getData() == 0) return;

    BitBoard demoRight(NOT_FILE_A);
    BitBoard demoLeft(NOT_FILE_H);

    // Check the spot in front of pawn:
    // If the pawn is white, we go back 8 spots in the bitboard to look up one row
    // If the pawn is black, we go forward 8 spots to look down one row.
    // We then mark that spot as a movable square.
    // This works because each row is 8 spots on the board.
    BitBoard singleMoves = (color == WHITE) ? (pawns.getData() << 8) & emptySquares.getData() : (pawns.getData() >> 8) & emptySquares.getData();
    
    // Check if pawns are currently at Row 3 or Row 6 (3rd for White, 6th for Black). These are the starting rows for each pawn.
    // Of course, if a pawn is in its starting row, it can move 2 rows instead of 1.
    // If any spots in that row have a pawn in them, then we mark the spots 2 rows forward or 2 rows backward.
    BitBoard doubleMoves = (color == WHITE) ? ((singleMoves.getData() & RANK_3) << 8) & emptySquares.getData() : ((singleMoves.getData() & RANK_6) >> 8) & emptySquares.getData();
    
    // Fo Left Captures:
    // Leaving out any pawns on the left File (column) of the board, we mark the diagonal-left spots for every pawn on the board.
    // For White, this is spot << 7, which is one row up and one column left. (adding 7 to it)
    // For Black, this is spot >> 9, which is one row down and one column left. (subtracting 9 from it)
            //Think of this as a backwards of going one row up and 
    BitBoard capturesLeft = (color == WHITE) ? ((pawns.getData() & NOT_FILE_A) << 7) & enemyPieces.getData() : ((pawns.getData() & NOT_FILE_A) >> 9) & enemyPieces.getData();
    
    // For Right Captures:
    // Just the inverse of left captures.
    BitBoard capturesRight = (color == WHITE) ? ((pawns.getData() & NOT_FILE_H) << 9) & enemyPieces.getData() : ((pawns.getData() & NOT_FILE_H) >> 7) & enemyPieces.getData();

    int shiftForward = (color == WHITE) ? 8 : -8;
    int doubleShift = (color == WHITE) ? 16 : -16;
    int captureLeftShift = (color == WHITE) ? 7 : -9;
    int captureRightShift = (color == WHITE) ? 9 : -7;

    addPawnBitboardMovesToList(moves, singleMoves, shiftForward);
    addPawnBitboardMovesToList(moves, doubleMoves, doubleShift);
    addPawnBitboardMovesToList(moves, capturesLeft, captureLeftShift);
    addPawnBitboardMovesToList(moves, capturesRight, captureRightShift);
}

void Chess::addPawnBitboardMovesToList(std::vector<BitMove>& moves, const BitBoard bitboard, int shift) {
    if (bitboard.getData() == 0) return;
    bitboard.forEachBit([&](int toSquare) {
        int fromSquare = toSquare - shift;
        moves.emplace_back(fromSquare, toSquare, ChessPiece::Pawn);
    });
}

#pragma endregion pawnMoves

#pragma region knightMoves

BitBoard Chess::generateKnightMoveBitBoard(int square) {
    BitBoard bitboard = 0ULL;
    int rank = square / 8;
    int file = square & 7;

    std::pair<int, int> knightOffsets[] = {
        { 2, 1 }, { 2, -1 }, { -2, 1 }, { -2, -1 },
        { 1, 2 }, { 1, -2 }, { -1, 2 }, { -1, -2 }
    };

    // IDK he had this code on the lecture LMAO lets see if i can explain this to myself
    // We find all possible spots that a knight could move to from each square.
    constexpr uint64_t oneBit = 1;
    // For each possible direction, we find the spot at each offset.
    // If that spot is still on the board, we add it to our final knight move BitBoard.
    for (auto [dr, df] : knightOffsets) {
        int r = rank + dr, f = file + df;
        if (r >= 0 && r < 8 && f >= 0 && f < 8) {
            bitboard |= oneBit << (r * 8 + f);
        }
    }

    return bitboard;
}

void Chess::generateKnightMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t emptySquares) {
    knightBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard = BitBoard(_knightBitBoards[fromSquare].getData() & emptySquares);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
}


#pragma endregion knightMoves

#pragma region kingMoves

BitBoard Chess::generateKingMoveBitBoard(int square) {
    BitBoard bitboard = 0ULL;
    int rank = square / 8;
    int file = square & 7;

    std::pair<int, int> kingOffsets[] = {
        { -1, 1 }, { 0, 1 }, { 1, 1 },
        {-1, 0 },            { 1, 0 },
        {-1, -1}, { 0, -1 }, { 1, -1 } 
    };

    constexpr uint64_t oneBit = 1;

    for (auto [dr, df] : kingOffsets) {
        int r = rank + dr, f = file + df;
        if (r >= 0 && r < 8 && f >= 0 && f < 8) {
            bitboard |= oneBit << (r * 8 + f);
        }
    }

    return bitboard;
}

void Chess::generateKingMoves(std::vector<BitMove>& moves, BitBoard kingBoard, uint64_t emptySquares) {
    kingBoard.forEachBit([&](int fromSquare){
        BitBoard moveBitBoard = BitBoard(_kingBitBoards[fromSquare].getData() & emptySquares);
        moveBitBoard.forEachBit([&](int toSquare){
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
}

#pragma endregion kingMoves

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayer = getCurrentPlayer() -> playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor != currentPlayer) return false;

    _grid -> forEachSquare([](ChessSquare* sq, int x, int y) {
        sq -> setHighlighted(false);
    });

    bool ret = false;
    ChessSquare* fromSquare = (ChessSquare *)&src;
    if (fromSquare) {
        int fromIndex = fromSquare -> getSquareIndex();
        for (auto move : _moves) {
            if (move.from == fromIndex){
                ret = true;
                auto dest = _grid -> getSquareByIndex(move.to);
                dest -> setHighlighted(true);
            }   
        }
    } 
    return ret;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* fromSquare = (ChessSquare *)&src;
    ChessSquare* toSquare = (ChessSquare *)&dst;
    if (toSquare) {
        int toIndex = toSquare -> getSquareIndex();
        int fromIndex = fromSquare -> getSquareIndex();
        for (auto move : _moves) {
            if (move.to == toIndex && move.from == fromIndex){
                return true;
            }   
        }
    }
    return false;
}

void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    _grid->forEachSquare([](ChessSquare* sq, int x, int y) {
        sq->setHighlighted(false);
    });
    
    _moves = generateAllMoves();
    endTurn();
}

void Chess::addMoveIfValid(const char *state, std::vector<BitMove>& moves, int fromRow, int toRow, int fromCol, int toCol, ChessPiece piece){
    if (toRow >= 0 && toRow < 8 && toCol >= 0 && toCol < 8) {
        moves.emplace_back(fromRow * 8 + fromCol, toRow * 8 + toCol, piece);
    }
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;
}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, ChessPiece::Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}
