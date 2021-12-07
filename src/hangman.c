// Created by: Aditya Chaudhary and Ethan Bitnun for ECE198
// ST7789 drivers can be found at: https://github.com/Floyd-Fish/ST7789-STM32
// We have made any modifications necessary for our project.
// All other files are credited in the beginning.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "spi.h"
#include "gpio.h"
#include "fonts.h"
#include "pinmappings.h"
#include "st7789 drivers.h"
#include "stm32f4xx_hal.h"
#include "ece198.h"
#include <math.h>

// All functions were previously planned out in detail allowing us to create near if not fully functional copies immediately with 
// little testing in a burner file. 

//Git testing

// Functions
int main(void); // main function
void welcome(); // welcome message
void instructions(); // instructions
void goodbye(); // goodbye message
void pauseProgram(); // asks for a char input to pause the program
void menu(char[]); // gets users choice between instructions, quit, and play
void startGame(); // opens file and starts game if user selects to  play (calls menu)
bool isPresent(char, char[]); // checks to see if the char argument is present in the char[] argument
void play(char[]); // repeatedly gets user guesses and either reveals letters or removes lives (playing the game)
int addToGuessed(char, char[]); // returns the next free index in guessedLetters to add a new letter to guessedLetters
bool isRoundWon(char[], char[], int); // Checks if the word has been fully guessed
int getBinaryInput(int); // Gets input from the single button in binary and converts it to an ascii value
void strout(char[], int, int, int, int); // Allows for printing of strings greater than one line in 
                                         // length, around 28 characters, without cutting words in half when it hits the edge.
                                         // Used for printing large strings which require little formatting

// NOTE: Unfortunately the functions below were not able to be completed, as file system / manipulation with STM32 
// is very problematic as far as we've researched, partially due to little documentation. This makes us unable to save a config file as planned. 
// This is still ok as they were extra features which we were adding, and not in our design document or pitch.
void settings(); // Admin page accessible only by escape room owner with a password.
void settingsAccess(); // Password entry page to access settings or return to main menu.
int accessGranted(); // Checks to see if password is correct, incorrect, or if user wants to return to menu.

int main(void){
    HAL_Init(); // initialize the Hardware Abstraction Layer

    // Peripherals (including GPIOs) are disabled by default to save power, so we
    // use the Reset and Clock Control registers to enable the GPIO peripherals that we're using.

    __HAL_RCC_GPIOA_CLK_ENABLE(); // enable port A (for the on-board LED, for example)
    __HAL_RCC_GPIOB_CLK_ENABLE(); // enable port B (for the rotary encoder inputs, for example)
    __HAL_RCC_GPIOC_CLK_ENABLE(); // enable port C (for the on-board blue pushbutton, for example)

    // Setup for lcd display
    MX_GPIO_Init();
    MX_SPI1_Init();
    ST7789_Init();

    // set up for serial communication to the host computer
    // (anything we write to the serial port will appear in the terminal (i.e. serial monitor) in VSCode)
    SerialSetup(9600); // For testing only

    welcome(); // Welcome screen
    startGame(); // Starting the game
    return 0;
}

// Purpose: Retrieves input from one button rather than two, and differentiates between 1 and 0 bits by how long the button was held. 
//          All to eventually return an ascii value.
int getBinaryInput(int length){
    bool blue = false;
    int sum = 0; // total sum of all binary bits whihc is the ascii value
    int count = 0; // how many times to get input
    uint32_t pressTime = 0; // When the button is pressed
    uint32_t liveTime = 0; // When the button is released
    uint32_t totalPressTime = 0; // The difference between livetime and presstime, meaning i is how long the button was held down
    uint32_t colorTimerLive = 0; // Stores Live value of how long the button is held for the color output

    // This is done so that we can control how many inputs we get
    // For example, menu only needs two inputs, whereas play needs 7
    while (count < length){
        blue = false;
        colorTimerLive = 0;
        pressTime = 0; // Resets both values before getting more input
        liveTime = 0;
        
        // As soon as the button is pressed, this while loop terminates, hence giving the msot recent time for pressTime
        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)){
            pressTime = HAL_GetTick();
            
        }

        // Stops the program until after the user lets go of the button
        while (!HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)){
            colorTimerLive = HAL_GetTick();
            // Checks to see if the time between the initial press and current is less than 500 ms.
            if ((colorTimerLive - pressTime) < 500){
                //ST7789_DrawFilledCircle(300, 220, 10, RED); // Shows red circle for holding less than 500 ms, meaning 0 
                ST7789_WriteChar(5 + count*12, 220, '0', Font_11x18, RED, BLACK);
            }
            else if ((colorTimerLive - pressTime) >= 500 && (colorTimerLive - pressTime) < 2500){
                //ST7789_DrawFilledCircle(300, 220, 10, GREEN); // Shows green circle for holding more then 499 ms, meaning 1
                ST7789_WriteChar(5 + count*12, 220, '1', Font_11x18, GREEN, BLACK);
            }
            else{
                blue = true;
                ST7789_WriteChar(5 + count*12, 220, 'D', Font_11x18, BLUE, BLACK);
            }
        }
        //ST7789_DrawFilledCircle(300, 220, 10, BLACK); // Erase the hold length indicators
        // ST7789_WriteString(20, 220, "                           ", Font_11x18, BLACK, BLACK);
        if (blue){
            ST7789_WriteChar(5 + count*12, 220, 'D', Font_11x18, BLACK, BLACK);
        }

        liveTime = HAL_GetTick(); // Gets the time of button release as soon as the user releases it
        totalPressTime = liveTime - pressTime; // Finds the total length of time that the button was held

        // If the button was held for more than 1000 ms, binary bit is flipped on, and 
        // we add to the sum, else the binary bit is off and sum is untouched

        if (totalPressTime >= 500 && totalPressTime < 2500){
            // Adds the decimal value of whatever binary value it currently is, eventually 
            // summing up to a final ascii value after 8 inputs. decimal value = 2^count
            sum += pow(2, count);
            count++; // Increments count for the next input/binary bit
        }
        else if (totalPressTime < 500){
            
            // Increments count, and sum is untouched, as a 0 input was given.
            count++;
        }
    }
    ST7789_WriteString(0, 220, "                             ", Font_11x18, BLACK, BLACK);
    return sum; // Returns the final ascii value obtained
}

// Purpose: Actual Gameplay. Gets user input, processes guesses, and outputs gameplay to the user
void play(char words[]){

    // Variables
    int lives = 7;
    int length = 0; // The length of the current word being guessed
    char guess; // The users input
    char guessedLetters[26] = {0}; // Array which stores all the guessed letters
    char word[13];
    int wordNum = 0;
    int wordIndex = 0;


    while (lives != 0){

        // Clear guessedLetters and sets word length to 0 if its not the first time here and there are chars stored 
        // (necessary if the user passed the first word)
        length = 0;
        for (int i = 0 ; i < 26 ; i++){
            guessedLetters[i] = 0;
            if (i < 13){
                word[i] = 0;
            }
        }

        // This code block is responsible for filling the word array with each word after every round.
        wordIndex = 0; // Index for word
        // This will always iterate 13 times, however each time it will remember its position in the large words array
        // Takes sets of 13 characters and puts it into word array for each round
        for (int i = wordNum*13 ; i < (wordNum+1)*13 ; i++){
            word[wordIndex] = words[i];
            if (words[i+1] == ' '){
                wordNum++;
                break;
            }
            wordIndex++;
        }
        //strout(word, 10, 110, 13, 1); // For testing to see if word was acquired correctly
        pauseProgram();

        // CLear screen if user chooses to play
        ST7789_Fill_Color(BLACK);
        //fgets(word, 13, words); // get the next word from the file
        //printf("word: %s-", word);

        // Traverses through the first 13 characters of word
        for (int i = 0 ; i < 13 ; i++){

            // If word isnt any letter in the alphabet, upper or lower case, it will stop adding length to the word, meaning the word is finished
            if ( !((word[i] >= 65 && word[i] <= 90) || (word[i] >= 97 && word[i] <= 122)) ){
                break;
            }
            length++;
        }
        
        // Actual gameplay with guessing and output
        do{

            // Printing the word with non guessed letters hidden
            for (int i = 0 ; i < length ; i++){
                
                // if (word[i] == ' '){
                //     break;
                // }
                // If the letter in word[] is already guessed, output the letter instead of a star
                if (isPresent(word[i], guessedLetters)){
                    //printf("%c", word[i]);
                    ST7789_WriteChar(7 + 12*i, 10, word[i], Font_11x18, WHITE, BLACK);
                }
                else {
                    ST7789_WriteChar(7 + 12*i, 10, '*', Font_11x18, WHITE, BLACK);
                }
            }

            // Printing out what the player has already guessed and the lives
            //printf("\nGuessed: ", '\n');
            strout("Guessed: ", 7, 30, 10, 1);
            for (int i = 0 ; i < 26 ; i++){

                // If there is a non default entry in guessedletters, it must be a letter which was guessed
                if (guessedLetters[i] != 0){    
                    
                    // If its the first letter that was guessed, dont add a coma, otherwise add one to keep a clean look
                    //if (i == 0){
                        //printf("%c", guessedLetters[i]);
                        ST7789_WriteChar(110 + i*12, 30, guessedLetters[i], Font_11x18, WHITE, BLACK);
                    //}
                    //else{
                        //printf(", %c", guessedLetters[i]);
                        //ST7789_WriteString(127, 30 + i*12, guessedLetters[i], Font_11x18, WHITE, BLACK);
                    //}
                }
            }

            // Printing the users total lives
            //printf("\nLives: %i", lives, '\n');
            //printf("\n", '\n');
            ST7789_WriteString(7, 50, "Lives: ", Font_11x18, WHITE, BLACK);
            ST7789_WriteChar(103, 50, lives+48, Font_11x18, WHITE, BLACK);
            
            // User guessing
            //printf("Please enter your letter guess: ");
            //ST7789_WriteString(7, 70, "Please enter your guess [all 0 to exit]:", Font_11x18, WHITE, BLACK);
            strout("Please enter your guess or all 0 to exit:", 7, 70, 42, 3);
            guess = getBinaryInput(8);
            fflush(stdin);

            // If the guess was lower case, subtract 32 to make its ascii value the equivalent upper case letter
            if (guess == 0){
                break;
            }
            else if ( !((guess >= 65 && guess <= 90) || (guess >= 97 && guess <= 122)) ){
                lives--;
                ST7789_WriteString(7, 200, "Thats no letter!", Font_11x18, WHITE, BLACK);
            }
            else{
                if (guess > 90){
                    guess -=32;
                }

                // Checking if the guess is already present in guessed letters
                if (isPresent(guess, guessedLetters)){
                    ST7789_WriteString(7, 200, "You already guessed that!", Font_11x18, WHITE, BLACK);
                }
                else{

                    // Adds letter guess to guesedletters
                    guessedLetters[addToGuessed(guess, guessedLetters)] = guess;

                    // If the guess is present in the word, or not
                    if (isPresent(guess,word)){
                        ST7789_WriteString(7, 200, "Correct", Font_11x18, WHITE, BLACK);
                    }
                    else{
                        lives--;
                        ST7789_WriteString(7, 200, "Incorrect", Font_11x18, WHITE, BLACK);
                    }
                }
            }

            // Stopping the program to let the user see the correct or incorrect message
            pauseProgram(); 
            ST7789_Fill_Color(BLACK);

            // If the user guessed all the letters in the word
            if (isRoundWon(word, guessedLetters, length)){
                ST7789_WriteString(7, 130, "YOU GUESSED IT!", Font_11x18, WHITE, BLACK);
                pauseProgram();
                
                if (wordNum == 5){
                    ST7789_WriteString(7, 150, "YOU GUESSED IT ALL!!!!", Font_11x18, CYAN, BLACK);
                    break;
                }
                else{
                    strout("Enter 1 to stop. Enter 0 to continue playing.", 7, 130, 46, 3);
                    guess = getBinaryInput(1);
                    fflush(stdin);

                    if (guess == 1){
                        lives = 0;
                    }
                    else if(guess == 0){
                        guess = 3;
                    }
                    break;
                }
            }
        }
        while( (lives != 0) && !isRoundWon(word,guessedLetters,length));

        if (guess == 1 || guess == 0 || wordNum == 5){
            break;
        }
    }

    // close and open the file to restart at the beginning if the user is kicked from the game loops
    // (meaning the user lost all lives, or purposefully chose to restart)
    // fclose(words);
    // fopen("words.txt","r");
}

// Purpose: Checks to see if the char 'letter' passed in is found in the array 'guessesOrWord' passed in
bool isPresent(char letter, char guessesOrWord[26]){ // guessesOrWord array can either hold the guessed letters array, or the words array

    // Traverse through the guessesOrWord array
    for (int i = 0 ; i < 26 ; i++){

        // If the letter is found in the array, return true
        if (letter == guessesOrWord[i]){
            return true;
        }
        if (guessesOrWord[i+1] > 90 || guessesOrWord[i+1] < 65){ // Null character were causing issues with returning true for non-present charcters, hence this
            return false;                                        // if statement to recognize when the next letter is non-alphabet, and return false before then.
        }
    }
    return false;
}

// Purpose: returns the next free index of guessedletters to add letters to the array in the play function
int addToGuessed(char letter, char guessedLetters[]){
    // Variables
    int index = 0; // The next free index

    // Traverse through the guessedleters array
    for (int i = 0 ; i < 26 ; i++){
        
        // If index i of guessedletters is the default value, set the index to return as i, and break the loop
        if (guessedLetters[i] == 0){
            index = i;
            break;
        }
    }

    return index;
}

// Purpose: Returns true if the word has been fully guessed, false if otherwise
bool isRoundWon(char word[], char guessedLetters[], int length){

    // Variables
    int count = 0; // Indicates how many correctly guessed letters are in the word

    // Traverses through the length of the word
    for (int i = 0 ; i < length ; i++){

        // Traverses through the guessedletters array to see if word[i] is guessed or not
        for (int j = 0 ; j < 26 ; j++){

            // If the letter in word has been guessed, add 1 to the count of correct letters, and break the inner loop
            if (word[i] == guessedLetters[j]){
                count++; // counts how many letters in word are already guessed
                break; // break out of loop
            }
        }
    }

    // If the length of the word = the amount of correct guesses, game wins, otherwise returns false
    if (count == length){ 
        return true;
    }
    return false;
}

// Purpose: Setup the answer file and send the user to the menu
void startGame(){

    // Variables
    char word[65] = "PROFESSOR LAPTOP KNIFE NUCLEO PHYSICS "; // This will hold the words in the game because files do not work properly with STM32 Nucleo
    //char word[65] = "PP LL K K P "; // Fast and easy testing word
    
    // All file code below is N/A, beyond scope of the course, very difficult
    // FILE *words = fopen("D:\\words.txt", "r"); 
    // //Checking to see if file open was a success, if not, output error
    // if (words == NULL){
    //     SerialPuts("ERROR OPENING FILE");
    // }

    menu(word); // Continue to the main menu
    
}

// Purpose: Act as the menu for the player. Gives options on what to do, like play, quit, and instructions.
//          Correctly gets user input and sends the user to where they choose
void menu(char word[]){
    // Variables
    char guess; // The users input on what to do

    // While the guess is not q or Q, aka: while the user does not want to quit
    while (guess != 0){
        
        ST7789_Fill_Color(BLACK); // MUST REMOVE THIS FOR DEBUGGING. IT MAY HIDE USEFUL ERRORS IF NOT COMMENTED
        ST7789_WriteString(7, 10, "Enter 1-1 for Instructions", Font_11x18, WHITE, BLACK);
        ST7789_WriteString(7, 30, "Enter 0-0 to Quit :(", Font_11x18, WHITE, BLACK);
        ST7789_WriteString(7, 50, "Enter 0-1 to Play :D", Font_11x18, WHITE, BLACK);
        //ST7789_WriteString(7, 70, "Enter 10 for Settings", Font_11x18, WHITE, BLACK); // Unfortunately settings were not done, see function declarations for why
        //strout("Enter 11 for Instructions - Enter 00 for Quit - Enter 01 for Play", 7, 10, 69, 4);
        
        ST7789_WriteString(7, 110, "Short press ", Font_11x18, WHITE, BLACK);
        ST7789_WriteString(151, 110, "(RED)", Font_11x18, RED, BLACK);
        ST7789_WriteString(211, 110, " for 0", Font_11x18, WHITE, BLACK);

        ST7789_WriteString(7, 130, "Long press ", Font_11x18, WHITE, BLACK);
        ST7789_WriteString(139, 130, "(GREEN)", Font_11x18, GREEN, BLACK);
        ST7789_WriteString(223, 130, " for 1", Font_11x18, WHITE, BLACK);

        ST7789_WriteString(7, 150, "Hold press ", Font_11x18, WHITE, BLACK);
        ST7789_WriteString(139, 150, "(BLUE)", Font_11x18, BLUE, BLACK);
        ST7789_WriteString(211, 150, " to redo", Font_11x18, WHITE, BLACK);

        strout("Please enter your choice: ", 7, 190, 27, 1);
        
        guess = getBinaryInput(2);

        // If statement to determine where to send the user, either sends user to a function, or breaks the menu loop
        if (guess == 3){
            instructions();
        }
        else if (guess == 0){
            break;
        }
        else if (guess == 2){
            play(word);
        }
        else if (guess == 1){

        }
        else{
            strout("Invalid input!!! Try again.", 7, 220, 28, 1);
            pauseProgram();
        }
    }
    // If the user wanted to quit, the loop will break and goodbye() will trigger
    goodbye();
}

// Purpose: Outputs the instructions of the game to the user
void instructions(){

    ST7789_Fill_Color(BLACK);

    char sentence[30] = "Instructions:";
    ST7789_WriteString(7, 10, sentence, Font_11x18, WHITE, BLACK);

    strout("1. You will have to guess letters to try and complete the word hidden under the stars.", 7, 50, 87, 7);
    pauseProgram();

    strout("2. If there are multiple instances of the same letter, they all are revealed upon guessing.", 7, 50, 92, 7);
    pauseProgram();

    strout("3. Any incorrect guess deducts a life, once all lives are depleted, the game ends.", 7, 50, 83, 7);
    pauseProgram();

    strout("4. If you guess the word before losing all your lives, you can move to a new word with your remaining lives.", 7, 50, 109, 7);
    pauseProgram();
}

// Purpose: Prints strings greater than one line in length, while moving to the next line when letters move off the page
void strout(char sentence[], int x, int y, int size, int lines){

    // Index in the sentence
    int spot = 0;

    // Clearing as many lines as passed in by the programmer
    for (int i = 0 ; i < lines ; i++){
        ST7789_WriteString(x, y+i*20, "                           ", Font_11x18, BLACK, BLACK); // Empty line, awkward but works
    }

    // This embedded for-loop prints sentence[] character by character, moving 
    // to the next line if it detects that it may go off screen

    // This loop controls the y_shift, 20 is slightly more than the height of a character
    for (int y_shift = 0 ; y_shift < size ; y_shift += 20){

        // This loop controls the x_shift, 12 is slightly more than the width of a character
        for (int x_shift = 0 ; x_shift < 300 ; x_shift += 12){

            // Outputting the character in the correct area according to the loops and passed in x,y values
            ST7789_WriteChar(x + x_shift, y + y_shift, sentence[spot], Font_11x18, WHITE, BLACK);

            // If there is a space in the sentence, at the same time when there are less 
            // than 7 characters from the border, break in order to stop printing, and increase y_shift
            if (sentence[spot] == ' ' && (x + x_shift >= 224)){
                break;
            }

            // Detecting if the final character has been reached and breaking the loop
            if (spot == size - 2){
                break;
            }
            spot++; // Incrementing spot to get the next character in sentence[]
        }
        // Detecting if the final character has been reached and breaking the loop
        if (spot == size - 2){
            break;
        }
    }
}

// Purpose: Pauses the entire program by asking for user input to continue
void pauseProgram(){

    char sentence[30] = "Press blue to continue...";
    ST7789_WriteString(7, 220, sentence, Font_11x18, WHITE, BLACK);
    //printf("\nEnter any key to continue!\n");
    while(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13));
    ST7789_WriteString(7, 220, "                         ", Font_11x18, WHITE, BLACK);
    //fflush(stdin);
}

// Purpose: Outputs welcome message and asks to continue
void welcome(){
    // Variables for easy movement of the Titles as one group
    int xshift = -40;
    int yshift = 0;

    ST7789_WriteString(80 + xshift, 40 + yshift, "Welcome", Font_16x26, LIGHTBLUE, BLACK);
    ST7789_WriteString(110 + xshift, 70 + yshift, "to", Font_16x26, LIGHTBLUE, BLACK);
    ST7789_WriteString(140 + xshift, 100 + yshift, "Binary", Font_16x26, LIGHTBLUE, BLACK);
    ST7789_WriteString(170 + xshift, 130 + yshift, "HANGMAN!!!", Font_16x26, CYAN, BLACK);
    pauseProgram();
}

// Purpose: Outputs goodbye message
void goodbye(){
    ST7789_Fill_Color(BLACK);
    strout("Goodbye! We hope you enjoyed and got valuable clues!!!", 7, 10, 55, 3);
    strout("By: Aditya Chaudhary and Ethan Bitnun", 7, 90, 38, 2);
}

