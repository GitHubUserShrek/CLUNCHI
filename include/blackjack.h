#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

enum BJGameState
{
    BJ_START = 0,
    BJ_PLAYER_TURN,
    BJ_DEALER_TURN,
    BJ_GAME_OVER
};

struct Card
{
    uint8_t rank;
    uint8_t suit;
};

class Blackjack
{
public:
    Blackjack(U8G2 &u8g2) : u8g2_(u8g2) {}

    void hit();
    void stand();
    void updateAndDraw(bool tiltMode);
    void reset();

private:
    U8G2 &u8g2_;

    BJGameState state_ = BJ_START;

    Card playerHand_[7];
    int playerCount_ = 0;

    Card dealerHand_[7];
    int dealerCount_ = 0;

    Card deck_[52];
    int deckIndex_ = 0;

    uint32_t dealerTimer_ = 0;
    String gameOverMsg_ = "";

    void shuffleDeck();
    Card drawCard();
    int calculateHand(const Card *hand, int count);

    void drawMiniSuit(int x, int y, uint8_t suit);
    void drawPlayerCard(int x, int y, Card c);
};