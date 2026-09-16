#include "ClassicGames.h"

namespace {

GameRecord makeGame(const char *white, const char *black, const char *event, const char *site,
                    const char *date, const char *result, const char *eco,
                    std::initializer_list<MoveRecord> moves)
{
    GameRecord g;
    g.white = QString::fromUtf8(white);
    g.black = QString::fromUtf8(black);
    g.event = QString::fromUtf8(event);
    g.site = QString::fromUtf8(site);
    g.date = QString::fromUtf8(date);
    g.round = QStringLiteral("?");
    g.result = QString::fromUtf8(result);
    g.eco = QString::fromUtf8(eco);
    g.moves = QList<MoveRecord>(moves);
    g.plyCount = int(g.moves.size());
    return g;
}

} // namespace

QList<GameRecord> classicGames()
{
    QList<GameRecord> games;

    games << makeGame("Morphy, Paul", "Duke Karl / Count Isouard", "Paris Opera", "Paris FRA",
                      "1858.??.??", "1-0", "C41",
                      {{"e4", "e2e4"}, {"e5", "e7e5"}, {"Nf3", "g1f3"}, {"d6", "d7d6"},
                       {"d4", "d2d4"}, {"Bg4", "c8g4"}, {"dxe5", "d4e5"}, {"Bxf3", "g4f3"},
                       {"Qxf3", "d1f3"}, {"dxe5", "d6e5"}, {"Bc4", "f1c4"}, {"Nf6", "g8f6"},
                       {"Qb3", "f3b3"}, {"Qe7", "d8e7"}, {"Nc3", "b1c3"}, {"c6", "c7c6"},
                       {"Bg5", "c1g5"}, {"b5", "b7b5"}, {"Nxb5", "c3b5"}, {"cxb5", "c6b5"},
                       {"Bxb5+", "c4b5"}, {"Nbd7", "b8d7"}, {"O-O-O", "e1c1"}, {"Rd8", "a8d8"},
                       {"Rxd7", "d1d7"}, {"Rxd7", "d8d7"}, {"Rd1", "h1d1"}, {"Qe6", "e7e6"},
                       {"Bxd7+", "b5d7"}, {"Nxd7", "f6d7"}, {"Qb8+", "b3b8"}, {"Nxb8", "d7b8"},
                       {"Rd8#", "d1d8"}});

    games << makeGame("Anderssen, Adolf", "Kieseritzky, Lionel", "London casual game",
                      "London ENG", "1851.06.21", "1-0", "C33",
                      {{"e4", "e2e4"}, {"e5", "e7e5"}, {"f4", "f2f4"}, {"exf4", "e5f4"},
                       {"Bc4", "f1c4"}, {"Qh4+", "d8h4"}, {"Kf1", "e1f1"}, {"b5", "b7b5"},
                       {"Bxb5", "c4b5"}, {"Nf6", "g8f6"}, {"Nf3", "g1f3"}, {"Qh6", "h4h6"},
                       {"d3", "d2d3"}, {"Nh5", "f6h5"}, {"Nh4", "f3h4"}, {"Qg5", "h6g5"},
                       {"Nf5", "h4f5"}, {"c6", "c7c6"}, {"g4", "g2g4"}, {"Nf6", "h5f6"},
                       {"Rg1", "h1g1"}, {"cxb5", "c6b5"}, {"h4", "h2h4"}, {"Qg6", "g5g6"},
                       {"h5", "h4h5"}, {"Qg5", "g6g5"}, {"Qf3", "d1f3"}, {"Ng8", "f6g8"},
                       {"Bxf4", "c1f4"}, {"Qf6", "g5f6"}, {"Nc3", "b1c3"}, {"Bc5", "f8c5"},
                       {"Nd5", "c3d5"}, {"Qxb2", "f6b2"}, {"Bd6", "f4d6"}, {"Bxg1", "c5g1"},
                       {"e5", "e4e5"}, {"Qxa1+", "b2a1"}, {"Ke2", "f1e2"}, {"Na6", "b8a6"},
                       {"Nxg7+", "f5g7"}, {"Kd8", "e8d8"}, {"Qf6+", "f3f6"}, {"Nxf6", "g8f6"},
                       {"Be7#", "d6e7"}});

    games << makeGame("Anderssen, Adolf", "Dufresne, Jean", "Berlin casual game", "Berlin GER",
                      "1852.??.??", "1-0", "C52",
                      {{"e4", "e2e4"}, {"e5", "e7e5"}, {"Nf3", "g1f3"}, {"Nc6", "b8c6"},
                       {"Bc4", "f1c4"}, {"Bc5", "f8c5"}, {"b4", "b2b4"}, {"Bxb4", "c5b4"},
                       {"c3", "c2c3"}, {"Ba5", "b4a5"}, {"d4", "d2d4"}, {"exd4", "e5d4"},
                       {"O-O", "e1g1"}, {"d3", "d4d3"}, {"Qb3", "d1b3"}, {"Qf6", "d8f6"},
                       {"e5", "e4e5"}, {"Qg6", "f6g6"}, {"Re1", "f1e1"}, {"Nge7", "g8e7"},
                       {"Ba3", "c1a3"}, {"b5", "b7b5"}, {"Qxb5", "b3b5"}, {"Rb8", "a8b8"},
                       {"Qa4", "b5a4"}, {"Bb6", "a5b6"}, {"Nbd2", "b1d2"}, {"Bb7", "c8b7"},
                       {"Ne4", "d2e4"}, {"Qf5", "g6f5"}, {"Bxd3", "c4d3"}, {"Qh5", "f5h5"},
                       {"Nf6+", "e4f6"}, {"gxf6", "g7f6"}, {"exf6", "e5f6"}, {"Rg8", "h8g8"},
                       {"Rad1", "a1d1"}, {"Qxf3", "h5f3"}, {"Rxe7+", "e1e7"}, {"Nxe7", "c6e7"},
                       {"Qxd7+", "a4d7"}, {"Kxd7", "e8d7"}, {"Bf5+", "d3f5"}, {"Ke8", "d7e8"},
                       {"Bd7+", "f5d7"}, {"Kf8", "e8f8"}, {"Bxe7#", "a3e7"}});

    return games;
}
