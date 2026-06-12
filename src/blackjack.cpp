#include "blackjack.h"
#include "card_bitmaps.h"
#include "audio.h"
#include "logo_bitmaps.h"

extern Audio audio;

void Blackjack::reset()
{
    state_ = BJ_START;
    playerCount_ = 0;
    dealerCount_ = 0;
    deckIndex_ = 0;
    gameOverMsg_ = "";
}

void Blackjack::shuffleDeck()
{
    for (int i = 0; i < 52; i++)
    {
        deck_[i].rank = i % 13;
        deck_[i].suit = i / 13;
    }
    for (int i = 51; i > 0; i--)
    {
        int j = random(i + 1);
        Card temp = deck_[i];
        deck_[i] = deck_[j];
        deck_[j] = temp;
    }
    deckIndex_ = 0;
}

Card Blackjack::drawCard()
{
    if (deckIndex_ >= 52)
        shuffleDeck();
    return deck_[deckIndex_++];
}

int Blackjack::calculateHand(const Card *hand, int count)
{
    int total = 0;
    int aces = 0;

    for (int i = 0; i < count; i++)
    {
        if (hand[i].rank == 0)
        {
            aces++;
            total += 11;
        }
        else if (hand[i].rank >= 9)
        {
            total += 10;
        }
        else
        {
            total += (hand[i].rank + 1);
        }
    }

    while (total > 21 && aces > 0)
    {
        total -= 10;
        aces--;
    }
    return total;
}

void Blackjack::hit()
{
    if (state_ == BJ_START || state_ == BJ_GAME_OVER)
    {
        shuffleDeck();
        playerCount_ = 0;
        dealerCount_ = 0;

        playerHand_[playerCount_++] = drawCard();
        dealerHand_[dealerCount_++] = drawCard();
        playerHand_[playerCount_++] = drawCard();
        dealerHand_[dealerCount_++] = drawCard();

        state_ = BJ_PLAYER_TURN;
        audio.beep(1000, 30);

        if (calculateHand(playerHand_, playerCount_) == 21)
        {
            state_ = BJ_DEALER_TURN;
            dealerTimer_ = millis();
        }
    }
    else if (state_ == BJ_PLAYER_TURN && playerCount_ < 7)
    {
        playerHand_[playerCount_++] = drawCard();
        audio.beep(800, 20);

        if (calculateHand(playerHand_, playerCount_) >= 21)
        {
            state_ = BJ_DEALER_TURN;
            dealerTimer_ = millis();
        }
    }
}

void Blackjack::stand()
{
    if (state_ == BJ_PLAYER_TURN)
    {
        state_ = BJ_DEALER_TURN;
        dealerTimer_ = millis();
        audio.beep(1000, 30);
    }
}

void Blackjack::updateAndDraw(bool tiltMode)
{
    uint32_t now = millis();

    if (state_ == BJ_DEALER_TURN && now - dealerTimer_ > 1000)
    {
        dealerTimer_ = now;
        int pTotal = calculateHand(playerHand_, playerCount_);
        int dTotal = calculateHand(dealerHand_, dealerCount_);

        if (dTotal < 17 && pTotal <= 21 && dealerCount_ < 7)
        {
            dealerHand_[dealerCount_++] = drawCard();
            audio.beep(600, 30);
        }
        else
        {
            state_ = BJ_GAME_OVER;
            if (pTotal > 21)
            {
                gameOverMsg_ = "BUST!";
                audio.beep(200, 150);
            }
            else if (dTotal > 21)
            {
                gameOverMsg_ = "DEALER\nBUST!";
                audio.beep(1200, 50);
                delay(50);
                audio.beep(1500, 80);
            }
            else if (pTotal > dTotal)
            {
                gameOverMsg_ = "YOU WIN!";
                audio.beep(1200, 50);
                delay(50);
                audio.beep(1500, 80);
            }
            else if (pTotal < dTotal)
            {
                gameOverMsg_ = "DEALER\nWINS";
                audio.beep(200, 150);
            }
            else
            {
                gameOverMsg_ = "PUSH\n(TIE)";
                audio.beep(600, 50);
            }
        }
    }

    u8g2_.setDrawColor(1);
    u8g2_.setFont(u8g2_font_5x7_tr);

    if (state_ == BJ_START)
    {
        u8g2_.drawXBMP(14, 12, 100, 48, blackjack_logo_100x48);

        u8g2_.setFont(u8g2_font_7x14B_tr);
        int titleW = u8g2_.getStrWidth("BLACKJACK");
        int titleX = (128 - titleW) / 2;

        u8g2_.setDrawColor(0);
        u8g2_.drawBox(titleX - 4, 1, titleW + 8, 13);
        u8g2_.setDrawColor(1);
        u8g2_.drawStr(titleX, 12, "BLACKJACK");
    }
    else
    {
        int dTotal = calculateHand(dealerHand_, dealerCount_);
        char dBuf[32];
        static const char *ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

        int cardSpacing = (dealerCount_ > 4) ? 13 : ((dealerCount_ == 4) ? 16 : 20);
        int startX = 58;

        if (state_ == BJ_PLAYER_TURN)
        {
            u8g2_.drawStr(4, 12, "Dealer(?)");
            u8g2_.drawStr(startX, 12, "[?]");

            int cx = startX + cardSpacing;
            const char *rStr = ranks[dealerHand_[1].rank];
            u8g2_.drawStr(cx, 12, rStr);
            int rw = u8g2_.getStrWidth(rStr);
            drawMiniSuit(cx + rw + 1, 6, dealerHand_[1].suit);
        }
        else
        {
            snprintf(dBuf, sizeof(dBuf), "Dealer (%d)", dTotal);
            u8g2_.drawStr(4, 12, dBuf);

            for (int i = 0; i < dealerCount_; i++)
            {
                int cx = startX + (i * cardSpacing);
                const char *rStr = ranks[dealerHand_[i].rank];
                int rw = u8g2_.getStrWidth(rStr);

                u8g2_.setDrawColor(0);
                u8g2_.drawBox(cx - 1, 4, rw + 10, 10);
                u8g2_.setDrawColor(1);

                u8g2_.drawStr(cx, 12, rStr);
                drawMiniSuit(cx + rw + 1, 6, dealerHand_[i].suit);
            }
        }

        u8g2_.drawHLine(0, 16, 128);

        for (int i = 0; i < playerCount_; i++)
        {
            int cardX = 6 + (i * 15);
            drawPlayerCard(cardX, 22, playerHand_[i]);
        }

        int pTotal = calculateHand(playerHand_, playerCount_);
        char pBuf[32];
        snprintf(pBuf, sizeof(pBuf), "You: %d", pTotal);
        int pW = u8g2_.getStrWidth(pBuf);
        u8g2_.drawStr(128 - pW - 4, 26, pBuf);

        if (state_ == BJ_GAME_OVER)
        {
            int nlIdx = gameOverMsg_.indexOf('\n');
            if (nlIdx > 0)
            {
                String l1 = gameOverMsg_.substring(0, nlIdx);
                String l2 = gameOverMsg_.substring(nlIdx + 1);
                int w1 = u8g2_.getStrWidth(l1.c_str());
                int w2 = u8g2_.getStrWidth(l2.c_str());
                u8g2_.drawStr(128 - w1 - 4, 35, l1.c_str());
                u8g2_.drawStr(128 - w2 - 4, 44, l2.c_str());
            }
            else
            {
                int msgW = u8g2_.getStrWidth(gameOverMsg_.c_str());
                u8g2_.drawStr(128 - msgW - 4, 38, gameOverMsg_.c_str());
            }
        }
    }

    u8g2_.setFont(u8g2_font_5x7_tr);
    if (state_ == BJ_START || state_ == BJ_GAME_OVER)
    {
        if (state_ == BJ_START)
        {
            u8g2_.setDrawColor(0);
            u8g2_.drawBox(2, 54, 124, 9);
            u8g2_.setDrawColor(1);
        }
        u8g2_.drawStr(4, 62, "Hold:Back");
        u8g2_.drawStr(82, 62, "Tap:Deal");
    }
    else if (state_ == BJ_PLAYER_TURN)
    {
        u8g2_.drawStr(4, 62, "Tap:Hit");
        int w = u8g2_.getStrWidth("x2:Stand");
        u8g2_.drawStr(128 - w - 4, 62, "x2:Stand");
    }
    else
    {
        int w = u8g2_.getStrWidth("Dealer Turn...");
        u8g2_.drawStr((128 - w) / 2, 62, "Dealer Turn...");
    }

    u8g2_.drawRFrame(0, 0, 128, 64, 6);
}

void Blackjack::drawMiniSuit(int x, int y, uint8_t suit)
{
    const unsigned char *bmp = nullptr;
    switch (suit)
    {
    case 0:
        bmp = suit_spade_7x7;
        break;
    case 1:
        bmp = suit_heart_7x7;
        break;
    case 2:
        bmp = suit_diamond_7x7;
        break;
    case 3:
        bmp = suit_club_7x7;
        break;
    }
    if (bmp)
        u8g2_.drawXBMP(x, y, 7, 7, bmp);
}

void Blackjack::drawPlayerCard(int x, int y, Card c)
{
    u8g2_.setDrawColor(1);
    u8g2_.drawXBMP(x, y, 22, 30, card_frame_22x30);

    static const char *ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    const char *rankStr = ranks[c.rank];

    u8g2_.setFont(u8g2_font_6x12_tr);
    u8g2_.drawStr(x + 3, y + 11, rankStr);

    drawMiniSuit(x + 3, y + 15, c.suit);
}