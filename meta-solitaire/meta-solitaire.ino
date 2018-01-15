#include <Gamebuino-Meta.h>
#include "sprites.h"
#include "pile.h"
#include "undo.h"
#include "utils.h"

#define MAX_CARDS_DRAWN_IN_PILE 15
#define REPEAT_FRAMES 6

enum GameMode { dealing, selecting, drawingCards, movingPile, illegalMove, fastFoundation, wonGame };
// State of the game.
GameMode mode = dealing;
GameMode prevMode;

void drawDealing();
void drawCursor();
void drawDrawingCards();
void drawMovingPile();
void drawIllegalMove();
void drawWonGame();
typedef void(*DrawFunction)();
DrawFunction drawJumpTable[] = { drawDealing, drawCursor, drawDrawingCards, drawMovingPile, drawIllegalMove, drawIllegalMove, drawWonGame };

void handleSelectingButtons();
void handleMovingPileButtons();
void handleCommonButtons();
typedef void(*ButtonHandler)();
ButtonHandler buttonJumpTable[] = { handleCommonButtons, handleSelectingButtons, handleCommonButtons, handleMovingPileButtons, handleCommonButtons, handleCommonButtons, handleCommonButtons };

// Stock: where you draw cards from
// Talon: drawn cards
// Foundation: get the cards here in suit order to win
// Tableau: alternating colors, descending order
enum Location { stock, talon, 
  foundation1, foundation2, foundation3, foundation4,
  tableau1, tableau2, tableau3, tableau4, tableau5, tableau6, tableau7 }; 

// Stack that the cursor is currently pointed at.
Location activeLocation;
// Within the stack, card position for the cursor, 0 being top card.
byte cardIndex;
// Position of the cursor for animation.
byte cursorX, cursorY;

// Animating moving stack of cards.
Pile moving = Pile(13);
byte remainingDraws;
// 3 at a time for hard, 1 at a time for easy
byte cardsToDraw = 1;

// Keep track of source pile for returning invalid moves.
Pile *sourcePile;

Pile stockDeck = Pile(52), talonDeck = Pile(24);
Pile foundations[4] = { Pile(13), Pile(13), Pile(13), Pile(13) };
Pile tableau[7] = { Pile(20), Pile(20), Pile(20), Pile(20), Pile(20), Pile(20), Pile(20) };

UndoStack undo;

struct CardAnimation {
  Card card;
  byte tableauIndex;
  int16_t x, y, destX, destY;
};

// Used to deal at the start of the game.
CardAnimation cardAnimations[28];
byte cardAnimationCount = 0;

struct CardBounce {
  Card card;
  int16_t x, y, xVelocity, yVelocity;
};

// Keeps track of a bouncing card for the winning animation.
CardBounce bounce;
byte bounceIndex;

int easyGameCount, easyGamesWon, hardGameCount, hardGamesWon;

const MultiLang easyOption[] = {
  { Gamebuino_Meta::LangCode::en, "Easy game" },
  { Gamebuino_Meta::LangCode::de, "Einfach" },
  { Gamebuino_Meta::LangCode::fr, "Facile" }
};
const MultiLang hardOption[] = {
  { Gamebuino_Meta::LangCode::en, "Hard game" },
  { Gamebuino_Meta::LangCode::de, "Hart" },
  { Gamebuino_Meta::LangCode::fr, "Difficile" }
};
const MultiLang* newGameMenu[2] = {
  easyOption,
  hardOption
};

const MultiLang resumeOption[] = {
  { Gamebuino_Meta::LangCode::en, "Resume" },
  { Gamebuino_Meta::LangCode::de, "Fortsetzen" },
  { Gamebuino_Meta::LangCode::fr, "Reprendre" }
};
const MultiLang newGameOption[] = {
  { Gamebuino_Meta::LangCode::en, "New game" },
  { Gamebuino_Meta::LangCode::de, "Neu" },
  { Gamebuino_Meta::LangCode::fr, "Nouveau" }
};
const MultiLang undoOption[] = {
  { Gamebuino_Meta::LangCode::en, "Undo" },
  { Gamebuino_Meta::LangCode::de, "Umkehren" },
  { Gamebuino_Meta::LangCode::fr, "Annuler" }
};
const MultiLang* pauseMenu[3] = {
  resumeOption,
  newGameOption,
  undoOption
};

const uint16_t patternA[] = {0x0045, 0x0118, 0x0000};
const uint16_t patternB[] = {0x0045, 0x0108, 0x0000};

void setup() {
  gb.begin();
  gb.setFrameRate(30);

  // Initialize positions of piles.
  for (int i = 0; i < 4; i++) {
    foundations[i].x = 35 + i * 11;
    foundations[i].y = 2;
    foundations[i].isTableau = false;
  }
  for (int i = 0; i < 7; i++) {
    tableau[i].x = i * 11 + 2;
    tableau[i].y = 18;
    tableau[i].isTableau = true;
  }
  stockDeck.x = 2;
  stockDeck.y = 2;
  stockDeck.isTableau = false;
  talonDeck.x = 13;
  talonDeck.y = 2;
  stockDeck.isTableau = false;

  setupNewGame();
  
  // testWinningAnimation();
}

void setupNewGame() {
  gb.lights.fill(BLACK);
  
  talonDeck.empty();
  stockDeck.newDeck();
  stockDeck.shuffle();
  for (int i = 0; i < 4; i++) {
    foundations[i].empty();
  }
  for (int i = 0; i < 7; i++) {
    tableau[i].empty();
  }

  // Put the cursor at the stock.
  activeLocation = stock;
  bool unused;
  getCursorDestination(cursorX, cursorY, unused);

  // Initialize the data structure to deal out the initial board.
  cardAnimationCount = 0;
  for (int i = 0; i < 7; i++) {
    for (int j = i; j < 7; j++) {
      Card card = stockDeck.removeTopCard();
      if (i == j) card.flip();
      cardAnimations[cardAnimationCount] = CardAnimation();
      cardAnimations[cardAnimationCount].x = 2;
      cardAnimations[cardAnimationCount].y = 2;
      cardAnimations[cardAnimationCount].destX = tableau[j].x;
      cardAnimations[cardAnimationCount].destY = tableau[j].y + 2 * i;
      cardAnimations[cardAnimationCount].tableauIndex = j;
      cardAnimations[cardAnimationCount].card = card;
      cardAnimationCount++;
    }
  }
  cardAnimationCount = 0;

  drawBoard();
  // Determine easy or hard game.
  cardsToDraw = menu(newGameMenu, 2, false) == 0 ? 1 : 3;
  mode = dealing; 
}

void testWinningAnimation() {
  // For debugging winning animation

  stockDeck.empty();
  for (int i = 0; i < 7; i++) {
    tableau[i].empty();
  }
  for (int suit = spade; suit <= diamond; suit++) {
    for (int value = ace; value <= king; value++) {
      foundations[suit].addCard(Card(static_cast<Value>(value), static_cast<Suit>(suit), false));
    }
  }

  initializeCardBounce();
  drawBoard(); 
  mode = wonGame;
}

void loop() {
  if (gb.update()) {
    // Handle key presses for various modes.
    buttonJumpTable[mode]();
    
    // Draw the board.
    if (mode != wonGame) {
      drawBoard();      
    }
    
    // Draw other things based on the current state of the game.
    drawJumpTable[mode]();

    // displayCpuLoad();
  }
}

void displayCpuLoad() {
  gb.display.setColor(WHITE);
  gb.display.fillRect(0, 0, 12, REPEAT_FRAMES);
    
  gb.display.setCursor(0, 0);
  gb.display.setColor(BLACK);
  gb.display.print(gb.getCpuLoad());
}

void handleCommonButtons() {
  if (gb.buttons.pressed(BUTTON_MENU)) {
    switch (menu(pauseMenu, (!undo.isEmpty() && mode == selecting) ? 3 : 2, true)) {
      case 1: 
        setupNewGame();
        return;
      case 2:
        performUndo();
        break;
    }
  }
}

void handleSelectingButtons() {
  // Handle buttons when user is using the arrow cursor to navigate.
  Location originalLocation = activeLocation;
  if (gb.buttons.repeat(BUTTON_RIGHT, REPEAT_FRAMES)) {
    if (activeLocation != foundation4 && activeLocation != tableau7) {
      activeLocation = (Location)(activeLocation + 1);
    }
  }
  if (gb.buttons.repeat(BUTTON_LEFT, REPEAT_FRAMES)) {
    if (activeLocation != stock && activeLocation != tableau1) {
      activeLocation = (Location)(activeLocation - 1);
    }
  }
  if (gb.buttons.repeat(BUTTON_DOWN, REPEAT_FRAMES)) {
    if (cardIndex > 0) {
      cardIndex--;
    }
    else {
      if (activeLocation < foundation1) activeLocation = (Location)(activeLocation + 6);
      else if (activeLocation <= foundation4) activeLocation = (Location)(activeLocation + 7);
    }
  }
  if (gb.buttons.repeat(BUTTON_UP, REPEAT_FRAMES)) {
    bool interPileNavigation = false;
    if (activeLocation >= tableau1 && activeLocation <= tableau7) {
      Pile *pile = getActiveLocationPile();
      if (pile->getCardCount() > cardIndex + 1 && !pile->getCard(cardIndex + 1).isFaceDown()) {
        cardIndex++;
        interPileNavigation = true;
      }
    }
    if (!interPileNavigation) {
      if (activeLocation > tableau2) activeLocation = (Location)(activeLocation - 7);
      else if (activeLocation >= tableau1) activeLocation = (Location)(activeLocation - 6);
    }
  }
  if (gb.buttons.pressed(BUTTON_B)) {
    if (activeLocation >= tableau1 || activeLocation == talon) {
      Pile *pile = getActiveLocationPile();
      if (pile->getCardCount() > 0) {
        Card card = pile->getCard(0);
        bool foundMatch = false;
        for (int i = 0; i < 4; i++) {
          if (foundations[i].getCardCount() == 0) {
            if (card.getValue() == ace) {
              foundMatch = true;
            }
          }
          else {
            Card card1 = foundations[i].getCard(0);
            Card card2 = pile->getCard(0);
            if (card1.getSuit() == card2.getSuit() && card1.getValue() + 1 == card2.getValue()) {
              foundMatch = true;
            }
          }
          if (foundMatch) {
            moving.empty();
            moving.x = pile->x;
            moving.y = cardYPosition(pile, 0);
            moving.addCard(pile->removeTopCard());
            sourcePile = &foundations[i];
            mode = fastFoundation;
            playSoundA();
            break;
          }
        }
      }
    }
  }
  else if (gb.buttons.pressed(BUTTON_A)) {
    switch (activeLocation) {
      case stock:
        if (stockDeck.getCardCount() != 0) {
          moving.empty();
          drawAndFlip(&stockDeck, &moving);
          moving.x = stockDeck.x;
          moving.y = stockDeck.y;
          remainingDraws = min(cardsToDraw - 1, stockDeck.getCardCount()); 
          mode = drawingCards;
          playSoundA();
        }
        else {
          while (talonDeck.getCardCount() != 0) {
            drawAndFlip(&talonDeck, &stockDeck);
          }
          UndoAction action;
          action.setFlippedTalon();
          undo.pushAction(action);
        }
        break;
      case talon:
      case foundation1:
      case foundation2:
      case foundation3:
      case foundation4:
      case tableau1:
      case tableau2:
      case tableau3:
      case tableau4:
      case tableau5:
      case tableau6:
      case tableau7:
        sourcePile = getActiveLocationPile();
        if (sourcePile->getCardCount() == 0) break;
        moving.empty();
        moving.x = sourcePile->x;
        moving.y = cardYPosition(sourcePile, 0) - 2 * cardIndex;
        sourcePile->removeCards(cardIndex + 1, &moving);
        mode = movingPile;
        playSoundA();
        break;
    }
  }
  if (originalLocation != activeLocation) cardIndex = 0;

  handleCommonButtons();
}

void handleMovingPileButtons() {
  // Handle buttons when user is moving a pile of cards.
  if (gb.buttons.repeat(BUTTON_RIGHT, REPEAT_FRAMES)) {
    if (activeLocation != foundation4 && activeLocation != tableau7) {
      activeLocation = (Location)(activeLocation + 1);
    }
  }
  if (gb.buttons.repeat(BUTTON_LEFT, REPEAT_FRAMES)) {
    if (activeLocation != talon && activeLocation != tableau1) {
      activeLocation = (Location)(activeLocation - 1);
    }
  }
  if (gb.buttons.repeat(BUTTON_DOWN, REPEAT_FRAMES)) {
    if (activeLocation == talon) activeLocation = tableau2;
    else if (activeLocation <= foundation4) activeLocation = (Location)(activeLocation + 7);
  }
  if (gb.buttons.repeat(BUTTON_UP, REPEAT_FRAMES)) {
    if (activeLocation >= tableau4) activeLocation = (Location)(activeLocation - 7);
    else if (activeLocation >= tableau1) activeLocation = talon;
  }
  if (gb.buttons.pressed(BUTTON_A)) {
    playSoundB();
    switch (activeLocation) {
      case talon:
        mode = illegalMove;
        break;
      case foundation1:
      case foundation2:
      case foundation3:
      case foundation4:
        {
          if (moving.getCardCount() != 1) {
            mode = illegalMove;
            break;
          }
          Pile *destinationFoundation = getActiveLocationPile();
          if (destinationFoundation->getCardCount() == 0) {
            if (moving.getCard(0).getValue() != ace) {
              mode = illegalMove;
              break;
            }
          }
          else {
            Card card1 = destinationFoundation->getCard(0);
            Card card2 = moving.getCard(0);
            if (card1.getSuit() != card2.getSuit() || card1.getValue() + 1 != card2.getValue()) {
              mode = illegalMove;
              break;
            }
          }
          moveCards();
          checkWonGame();
        }
        break;
      case tableau1:
      case tableau2:
      case tableau3:
      case tableau4:
      case tableau5:
      case tableau6:
      case tableau7:
        {
          Pile *destinationTableau = getActiveLocationPile();
          if (destinationTableau->getCardCount() > 0) {
            // Make sure that it is a decending value, alternating color.
            Card card1 = destinationTableau->getCard(0);
            Card card2 = moving.getCard(moving.getCardCount() - 1);
            if (card1.isRed() == card2.isRed() || card1.getValue() != card2.getValue() + 1) {
              mode = illegalMove;
              break;
            }
          }
          else {
            // You can only place kings in an empty tableau.
            Card card = moving.getCard(moving.getCardCount() - 1);
            if (card.getValue() != king) {
              mode = illegalMove;
              break;
            }
          }
        }
        moveCards();
        break;
    }
  }

  handleCommonButtons();
}

void moveCards() {
  Pile *pile = getActiveLocationPile();
  UndoAction action;
  action.source = sourcePile;
  action.destination = pile;
  action.setCardCount(moving.getCardCount());
  pile->addPile(&moving);
  mode = selecting;
  if (updateAfterPlay()) {
    action.setRevealed();
  }
  undo.pushAction(action);
}

bool updateAfterPlay() {
  bool result = revealCards();
  checkWonGame();
  cardIndex = 0;
  bool unused;
  getCursorDestination(cursorX, cursorY, unused);
  return result;
}

bool revealCards() {
  bool revealed = false;
  // Check for cards to reveal.
  for (int i = 0; i < 7; i++) {
    if (tableau[i].getCardCount() == 0) continue;
    Card card = tableau[i].removeTopCard();
    if (card.isFaceDown()) {
      card.flip();
      revealed = true;
    }
    tableau[i].addCard(card);
  }
  return revealed;
}

void checkWonGame() {
  // Check to see if all foundations are full
  if (foundations[0].getCardCount() == 13 && foundations[1].getCardCount() == 13 &&
    foundations[2].getCardCount() == 13 && foundations[3].getCardCount() == 13) {
      initializeCardBounce();
      drawBoard(); 
      mode = wonGame;
      if (cardsToDraw == 1) {
        easyGamesWon++;
        // writeEeprom(false);
      }
      else {
        hardGamesWon++;
        // writeEeprom(false);
      }
  }
}

void drawBoard() {
  // Background
  gb.display.drawImage(0, 0, backgroundImage);
  
  // Stock
  if (stockDeck.getCardCount() != 0) {
    drawCard(stockDeck.x, stockDeck.y, Card(ace, spade, true));
  }
  
  // Talon
  if (talonDeck.getCardCount() > 0) {
    gb.display.drawImage(talonDeck.x, talonDeck.y, talonBackgroundImage);
    drawCard(talonDeck.x + (min(3, talonDeck.getCardCount()) - 1) * 2, talonDeck.y, talonDeck.getCard(0));
  }
  
  // Foundations
  for (int i = 0; i < 4; i++) {
    if (foundations[i].getCardCount() != 0) {
      drawCard(foundations[i].x, foundations[i].y, foundations[i].getCard(0));
    }
  }
  
  // Tableau
  for (int i = 0; i < 7; i++) {
    drawPile(&tableau[i]);
  }
}

void drawDealing() {
  if (cardAnimationCount < 28 && gb.frameCount % 4 == 0) {
    cardAnimationCount++;
    playSoundA();
  }
  bool doneDealing = cardAnimationCount == 28;
  for (int i = 0; i < cardAnimationCount; i++) {
    if (cardAnimations[i].x != cardAnimations[i].destX || cardAnimations[i].y != cardAnimations[i].destY) {
      doneDealing = false;
      drawCard(cardAnimations[i].x, cardAnimations[i].y, cardAnimations[i].card);
      cardAnimations[i].x = updatePosition(cardAnimations[i].x, cardAnimations[i].destX);
      cardAnimations[i].y = updatePosition(cardAnimations[i].y, cardAnimations[i].destY);
      if (cardAnimations[i].x == cardAnimations[i].destX && cardAnimations[i].y == cardAnimations[i].destY) {
        tableau[cardAnimations[i].tableauIndex].addCard(cardAnimations[i].card);
      }
    }
  }
  if (doneDealing) mode = selecting;
}

void drawDrawingCards() {
  drawPile(&moving);
  moving.x = updatePosition(moving.x, talonDeck.x + 2);
  moving.y = updatePosition(moving.y, talonDeck.y);
  if (moving.x == talonDeck.x + 2 && moving.y == talonDeck.y) {
    talonDeck.addCard(moving.getCard(0));
    if (remainingDraws) {
      remainingDraws--;
      moving.empty();
      drawAndFlip(&stockDeck, &moving);
      moving.x = stockDeck.x;
      moving.y = stockDeck.y;
      playSoundA();
    }
    else {
      UndoAction action;
      action.setDraw();
      undo.pushAction(action);
      mode = selecting;
    }
  }
}

void drawMovingPile() {
  drawPile(&moving);
  Pile* pile = getActiveLocationPile();
  byte yDelta = 2;
  if (pile->isTableau) yDelta += 2 * pile->getCardCount();
  moving.x = updatePosition(moving.x, pile->x);
  moving.y = updatePosition(moving.y, pile->y + yDelta);  
}

void drawIllegalMove() {
  // Move the cards back to the source pile.
  byte yDelta = 0;
  if (sourcePile->isTableau) yDelta += 2 * sourcePile->getCardCount();
  moving.x = updatePosition(moving.x, sourcePile->x);
  moving.y = updatePosition(moving.y, sourcePile->y + yDelta); 
  drawPile(&moving);
  // Check to see if the animation is done
  if (moving.x == sourcePile->x && moving.y == sourcePile->y + yDelta) {
    sourcePile->addPile(&moving);
    bool revealed = updateAfterPlay();
    
    // Update undo stack if this was a fast move to the foundation.
    if (mode == fastFoundation) {
      UndoAction action;
      action.source = getActiveLocationPile();
      action.destination = sourcePile;
      action.setCardCount(1);
      if (revealed) action.setRevealed();
      undo.pushAction(action);
    }
    if (mode != wonGame) mode = selecting;
  }
}

void drawWonGame() {
  // Bounce the cards from the foundations, one at a time.

  // Apply gravity
  bounce.yVelocity += 0x0080;
  bounce.x += bounce.xVelocity;
  bounce.y += bounce.yVelocity;

  // If the card is at the bottom of the screen, reverse the y velocity and scale by 80%.
  if (bounce.y + (14 << 8) > gb.display.width() << 8) {
    bounce.y = (gb.display.height() - 14) << 8;
    bounce.yVelocity = -bounce.yVelocity * 4 / 5;
    playSoundB();
  }
  drawCard(bounce.x >> 8, bounce.y >> 8, bounce.card);

  int val = (bounce.y * 255)* 3 / ((gb.display.height() - 14) << 8);
  for (int i = 0; i < 4; i++) {
    Color c = hsvToRgb565(gb.frameCount % 256, 255, max(255 - abs(i * 255 - val), 0));
    gb.lights.drawPixel(0, i, c);
    gb.lights.drawPixel(1, i, c);
  }
  
  // Check to see if the current card is off the screen.
  if (bounce.x + (10 << 8) < 0 || bounce.x > gb.display.width() << 8) {
    if (!initializeCardBounce()) {
      gb.lights.fill(BLACK);
      setupNewGame();
    }
  }
}

bool initializeCardBounce() {
  // Return false if all the cards are done.
  if (foundations[bounceIndex].getCardCount() == 0) return false;
  // Pick the next card to animate, with a random initial velocity.
  bounce.card = foundations[bounceIndex].removeTopCard();
  bounce.x = foundations[bounceIndex].x << 8;
  bounce.y = foundations[bounceIndex].y << 8;
  bounce.xVelocity = (random(2) ? 1 : -1) * random(0x0100, 0x0200);
  bounce.yVelocity = -1 * random(0x0200);
  bounceIndex = (bounceIndex + 1) % 4;
  return true;
}

void drawCursor() {
  bool flipped;
  byte x, y;
  getCursorDestination(x, y, flipped);
  
  cursorX = updatePosition(cursorX, x);
  cursorY = updatePosition(cursorY, y);

  drawCursor(cursorX, cursorY, flipped);
}

void getCursorDestination(byte& x, byte& y, bool& flipped) {
  Pile* pile = getActiveLocationPile();

  switch (activeLocation) {
    case stock:
      x = pile->x + 9;
      y = pile->y + 4;
      flipped = false;
      break;
    case talon:
      x = pile->x + 9 + 2 * min(2, max(0, pile->getCardCount() - 1));
      y = pile->y + 4;
      flipped = false;
      break;
    case foundation1:
    case foundation2:
    case foundation3:
    case foundation4:
      x = pile->x - 7;
      y = pile->y + 4;
      flipped = true;
      break;
    case tableau1:
    case tableau2:
    case tableau3:
      x = pile->x + 9;
      y = (cardIndex == 0 ? 4 : -2) + cardYPosition(pile, cardIndex);
      flipped = false;
      break;
    case tableau4:
    case tableau5:
    case tableau6:
    case tableau7:
      x = pile->x - 7;
      y = (cardIndex == 0 ? 4 : -2) + cardYPosition(pile, cardIndex);
      flipped = true;
      break;
  }
}

void drawCursor(byte x, byte y, bool flipped) {
  if (flipped) {
    if (cardIndex == 0) {
      gb.display.drawImage(x, y, cursorImage, -7, 7);
    }
    else {
      gb.display.drawImage(x - 14, y - 1, cursorDetailsImage, -21, 9);
      Card card = getActiveLocationPile()->getCard(cardIndex);
      byte extraWidth = card.getValue() == ten ? 2 : 1;
      drawValue(x - 12 + extraWidth, y + 1, card.getValue(), card.isRed());
      drawSuit(x - 8 + extraWidth, y + 1, card.getSuit());
    }
  }
  else {
    if (cardIndex == 0) {
      gb.display.drawImage(x, y, cursorImage);
    }
    else {
      gb.display.drawImage(x, y - 1, cursorDetailsImage);
      Card card = getActiveLocationPile()->getCard(cardIndex);
      byte extraWidth = card.getValue() == ten ? 2 : 1;
      drawValue(x + 8 + extraWidth, y + 1, card.getValue(), card.isRed());
      drawSuit(x + 12 + extraWidth, y + 1, card.getSuit());
    }
  }
}

Pile* getActiveLocationPile() {
  switch (activeLocation) {
    case stock:
      return &stockDeck;
    case talon:
      return &talonDeck;
    case foundation1:
    case foundation2:
    case foundation3:
    case foundation4:
      return &foundations[activeLocation - foundation1];
    case tableau1:
    case tableau2:
    case tableau3:
    case tableau4:
    case tableau5:
    case tableau6:
    case tableau7:
      return &tableau[activeLocation - tableau1];
  }
}

byte updatePosition(byte current, byte destination) {
  if (current == destination) return current;

  byte delta = (destination - current) / 4;
  if (delta == 0 && ((gb.frameCount % 2) == 0)) delta = destination > current ? 1 : -1;
  return current + delta;
}

void drawPile(Pile* pile) {
  int baseIndex = max(0, pile->getCardCount() - MAX_CARDS_DRAWN_IN_PILE);
  for (int i = 0; i < min(pile->getCardCount(), MAX_CARDS_DRAWN_IN_PILE); i++) {
    byte cardIndex = pile->getCardCount() - i - 1 - baseIndex;
    if (cardIndex == 0) {
      drawCard(pile->x, pile->y + 2 * i, pile->getCard(cardIndex));
    }
    else {
      Card card = pile->getCard(cardIndex);
      cardTopSprite.setFrame(card.isFaceDown() ? 1 : 0);
      gb.display.drawImage(pile->x, pile->y + 2 * i, cardTopSprite);
    }
  }
}

byte cardYPosition(Pile *pile, byte cardIndex) {
  if (pile->isTableau) {
    if (cardIndex > MAX_CARDS_DRAWN_IN_PILE - 1) return pile->y;
    return pile->y + 2 * (min(pile->getCardCount(), MAX_CARDS_DRAWN_IN_PILE) - cardIndex - 1);
  }

  return pile->y;
}

void drawCard(int16_t x, int16_t y, Card card) {
  cardSprite.setFrame(card.isFaceDown() ? 1 : 0);
  gb.display.drawImage(x, y, cardSprite);
  
  if (card.isFaceDown()) return;

  drawSuit(x + 1, y + 2, card.getSuit());
  drawValue(x + 5, y + 7, card.getValue(), card.isRed());
}

void drawSuit(int16_t x, int16_t y, Suit suit) {
  suitSprite.setFrame(suit);
  gb.display.drawImage(x, y, suitSprite);
}

void drawValue(int16_t x, int16_t y, Value value, bool isRed) {
  valueSprite.setFrame(value + (isRed ? 14 : 0));
  gb.display.drawImage(x, y, valueSprite);
  
  if (value == ten) {
    valueSprite.setFrame(isRed ? 14 : 0);
    gb.display.drawImage(x - 4, y, valueSprite);
  }
}

void playSoundA() {
  gb.sound.play(patternA);
}

void playSoundB() {
  gb.sound.play(patternB);
}

void drawAndFlip(Pile *source, Pile *destination) {
  Card card = source->removeTopCard();
  card.flip();
  destination->addCard(card);
}

void performUndo() {
  // Make sure there is something to undo.
  if (!undo.isEmpty() && mode == selecting) {
    UndoAction action = undo.popAction();
    // Handle draw from stock.
    if (action.wasDraw()) {
      for (byte i = 0; i < cardsToDraw; i++) {
        drawAndFlip(&talonDeck, &stockDeck);
      }
    }
    // Handle flipped talon.
    else if (action.wasFlippedTalon()) {
      while (stockDeck.getCardCount() != 0) {
        drawAndFlip(&stockDeck, &talonDeck);
      }
    }
    // Handle moving cards from one pile to another.
    else {
      // Handle moved cards resulted in revealing another card.
      if (action.wasRevealed()) {
        drawAndFlip(action.source, action.source);
      }
      moving.empty();
      action.destination->removeCards(action.getCardCount(), &moving);
      action.source->addPile(&moving);
    }
    updateAfterPlay();
  }
}

