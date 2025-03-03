#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// #define DEBUG

#include "mmm_term_colors.h"
#include "mmm_hashset.h"

#define WORD_LENGTH 5
#define NUM_PLAYER_GUESSES 6
#define NUM_BUCKETS 243  // 3 to the power of WORD_LENGTH
#define BEST_WORD "ROATE"
#define FLUSH_STDIN do { char c; while ((c = getchar()) != '\n' && c != EOF); } while (0)
#define TO_LOWER(X) ((X) | 32)
#define TO_UPPER(X) ((X) & ~32)
#define GETLINE(X, N) \
    char X[N + 1]; \
    fgets(X, N, stdin); \
    {   char *p = strchr(X, '\n'); \
        if (p == NULL) FLUSH_STDIN; else *p = '\0'; \
    }

uint32_t hash(void *word_ptr) {
    uint32_t result = 0;
    for (char *word = (char *) word_ptr; *word != '\0'; word++) result = (result << 5) + (*word & 0b11111);
    return result;
}

void load_words(char *filename, hashset *set) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening '%s'!\n", filename);
        return;
    }
    char word[WORD_LENGTH + 2];
    int word_count = 0;
    while (fgets(word, WORD_LENGTH + 2, file)) {
        word[WORD_LENGTH] = '\0';
        set_add(word, set, 0);
    }
    printf("Loaded %d words from '%s'\n", set->size, filename);
    fclose(file);
}

/** 
 * @return A color-coding as a ternary number with 0 = black, 1 = yellow, 2 = green.
 * @param guess a five-letter 'guess' string
 * @param solution a five-letter string to match the guess against
 *
 * For example, if the solution is LOGIC, guessing LIMBO returns 190 (dec) = 21001 (ternary).
 */
uint32_t color_code(char *guess, char *solution) {
    uint32_t result = 0;
    uint32_t matched[WORD_LENGTH] = {0};
    uint32_t pow_of_3 = 1;

    for (int i = WORD_LENGTH - 1; i >= 0; i--, pow_of_3 *= 3) {
        if (guess[i] == solution[i]) {
            result += 2 * pow_of_3;
            matched[i] = 1;
        }
    }

    for (int i = 0; i < WORD_LENGTH; i++) {
        pow_of_3 /= 3;
        if (guess[i] == solution[i]) continue;
        for (int j = WORD_LENGTH - 1; j >= 0; j--) {
            if (guess[i] == solution[j] && !matched[j]) {
                result += pow_of_3;
                matched[j] = 1;
                break;
            }
        }
    }
    return result;
}

/**
 * Returns a new (sub-)list of words from `currList` which satisfy the `color_code` obtained by guessing the word `guess`.
 */
void filter_list(char *guess, hashset *current_list, uint32_t col_code) {
    FOR_ITEM_IN_SET(word, current_list,
        if (color_code(guess, word) != col_code) set_delete(word, current_list, 0);
    );
    set_resize(current_list);
}

void print_color(char *guess, uint32_t color_code) {
    uint32_t pow_of_3 = 1;
    for (int i = 1; i < WORD_LENGTH; i++) pow_of_3 *= 3;
    for (int i = 0; i < WORD_LENGTH; i++, color_code %= pow_of_3, pow_of_3 /= 3) {
        switch (color_code / pow_of_3) {
            case 0: fputs(RESET, stdout); break;
            case 1: fputs(BG_BR(Y), stdout); break;
            case 2: fputs(BG_BR(G), stdout); break;
        }
        printf(" %c "RESET" ", TO_UPPER(guess[i]));
    }
}

/**
 * Returns the sum of squares of all bucket sizes obtained by guessing `guess`, assuming that the solution is a
 * uniformly random word in `current_list`.
 */
uint32_t sum_squares(char *guess, hashset *current_list) {
    int buckets[NUM_BUCKETS] = {0};
    FOR_ITEM_IN_SET(word2, current_list,
        buckets[color_code(guess, word2)]++;
    );
    uint32_t sum_sq = 0;
    for (int i = 0; i < NUM_BUCKETS; i++)
        sum_sq += buckets[i] * buckets[i];
    return sum_sq;
}

/**
 * Returns the word which minimizes the expected length of the new list of words obtained by guessing it.
 */
char *best_word(hashset *current_list, hashset *all_words, hashset *solutions) {
    if (current_list->size <= 2) {
        hashset_iterator iter = get_hashset_iterator(current_list);
        return all_words->elems->ptr + set_has(set_iter_next(&iter), all_words, 0);
    }
    char *best = NULL;
    double min_sum_sq = solutions->size * solutions->size + 1;  // sentinel value
    uint32_t best_word_is_a_solution = 0;

    FOR_ITEM_IN_SET(word, all_words,
        uint32_t sum_sq = sum_squares(word, current_list);
        uint32_t this_word_is_a_solution;
        if (sum_sq < min_sum_sq || (sum_sq == min_sum_sq && !best_word_is_a_solution && (this_word_is_a_solution = set_has(word, current_list, 0)))) {
            best = word;
            min_sum_sq = sum_sq;
            best_word_is_a_solution = this_word_is_a_solution;
        }
    );
    return best;
}

void play_Wordle(hashset *all_words, hashset *solutions) {

    typedef struct {
        char word[6];
        uint32_t col_code;
        uint32_t num_poss;
        double expect;
        char best[6];
        double best_expect;
    } guess_info;

    #define INSTRUCTIONS_1 BG_BR(G)" T "RESET"raining mode on/off, "BG_BR(Y)" G "RESET"ive up this round, "BG_BR(G)" L "RESET"ist all possible words, "BG_BR(Y)" Q "RESET"uit to main menu."RESET
    static int training_mode = 0;
    int play_game = 1;

    fputs("\n", stdout);
    while (play_game) {
        puts(FG(Y)"Let's play Wordle! Try to guess a 5-letter word! :)\n"INSTRUCTIONS_1);
        char *solution = set_at((((uint64_t) rand() << 32) | rand()) % solutions->size, solutions);
        hashset *current_list = set_copy(solutions);
        guess_info guesses[NUM_PLAYER_GUESSES];
        int guess_idx = 0;

        while (guess_idx <= NUM_PLAYER_GUESSES && play_game) {
            fputs("\n", stdout);
            for (int i = 0; i < guess_idx; i++) {
                printf("Guess #%2d: ", i + 1);
                print_color(guesses[i].word, guesses[i].col_code);
                if (training_mode) printf("(%d possibilities) "FG_BR(M)"[E(X) = %.3f]"FG_BR(B)"  best: %s [E(X) = %.3f]\n"RESET, guesses[i].num_poss, guesses[i].expect, guesses[i].best, guesses[i].best_expect); else printf("\n"RESET);
            }
            if (guess_idx > 0 && guesses[guess_idx - 1].col_code == NUM_BUCKETS - 1) {  // player wins
                printf(FG_BR(M)"\nYou solved it in %d guesses! Well done :D\n\n"RESET, guess_idx);
                break;
            } else if (guess_idx == NUM_PLAYER_GUESSES) {
                puts(FG_BR(R)"\nYou ran out of guesses!! T.T"RESET);
                fputs(FG_BR(Y)"The answer was: ", stdout);
                print_color(solution, NUM_BUCKETS - 1);
                fputs("\n\n", stdout);
                break;
            }
            while (1) {
                printf("Guess #%2d:  ", guess_idx + 1);
                GETLINE(inp, 50);
                char guessword[WORD_LENGTH + 1] = {'\0'};
                int letter = 0;
                for (char *i = inp; *i != '\0'; i++) {
                    if (isspace(*i)) continue;
                    if (isalpha(*i)) {
                        if (letter >= WORD_LENGTH) goto bad_input;
                        guessword[letter++] = TO_UPPER(*i);
                        continue;
                    }
                    goto bad_input;
                }
                if (letter == 0) continue;
                if (letter == 1) {
                    if (guessword[0] == 'Q' || guessword[0] == 'G') {
                        fputs(FG_BR(Y)"\nThe answer was: ", stdout);
                        print_color(solution, NUM_BUCKETS - 1);
                        fputs("\n", stdout);
                        play_game = guessword[0] == 'G';
                        guess_idx = NUM_PLAYER_GUESSES + 1;
                        break;
                    } else if (guessword[0] == 'L') {
                        fputs(RESET FG_BR(G), stdout);
                        FOR_ITEM_IN_SET(word, current_list, printf("%s  ", word); );
                        fputs(RESET"\n", stdout);
                        break;
                    } else if (guessword[0] == 'T') {
                        training_mode ^= 1;
                        fputs(FG_BR(M)"Training mode ", stdout);
                        puts(training_mode ? FG_BR(G)"ON"RESET : FG_BR(R)"OFF"RESET);
                        break;
                    } else goto bad_input;
                } else if (letter != WORD_LENGTH || !set_has(guessword, all_words, 0)) {
                    goto bad_input;
                }
                // Valid guess made
                strcpy(guesses[guess_idx].word, guessword);
                guesses[guess_idx].col_code = color_code(guessword, solution);
                guesses[guess_idx].expect = sum_squares(guessword, current_list) / (double) current_list->size;
                strcpy(guesses[guess_idx].best, guess_idx == 0 ? BEST_WORD : best_word(current_list, all_words, solutions));
                guesses[guess_idx].best_expect = sum_squares(guesses[guess_idx].best, current_list) / (double) current_list->size;
                filter_list(guessword, current_list, guesses[guess_idx].col_code);
                guesses[guess_idx].num_poss = current_list->size;
                guess_idx++;
                break;

                bad_input: puts(FG_BR(R)"Not a valid guess! "RESET"See "FG_BR(B)"'wordle_guessables.txt'"RESET" for a list of guessable words.\n"
                                INSTRUCTIONS_1"\n");
            }
        }
        // free up stuff
        set_free(current_list);
    }
    puts(FG_BR(Y)"Bye, hope you had fun!! ^o^\n"RESET);
}


void AI_play(hashset *all_words, hashset *solutions) {

    typedef struct {
        char word[6];
        uint32_t col_code;
        uint32_t num_poss;
        double expect;
    } guess_info;
    
    #define INSTRUCTIONS_2 BG_BR(G)" L "RESET"ist all possible words, "BG_BR(Y)" Q "RESET"uit to main menu."RESET
    int play_game = 1;
    while (play_game) {
        puts(FG_BR(Y)"\nLet's play Wordle! I'll guess and you tell me the colors! :)\n"INSTRUCTIONS_2);
        hashset *current_list = set_copy(solutions);
        guess_info guesses[NUM_PLAYER_GUESSES];
        int guess_idx = 0, AI_win = 0;

        while (guess_idx <= NUM_PLAYER_GUESSES && play_game) {
            
            if (current_list->size == 0) {
                puts(FG_BR(R)"No words found that match above guesses! :(\n"FG_BR(Y)"Starting a new game..."RESET);
                break;
            }

            for (int i = 0; i < guess_idx; i++) {
                printf("Guess #%2d: ", i + 1);
                print_color(guesses[i].word, guesses[i].col_code);
                printf("(%d possibilities) "FG_BR(M)"[E(X) = %.3f]\n"RESET, guesses[i].num_poss, guesses[i].expect);
            }

            if (guess_idx > 0 && guesses[guess_idx - 1].col_code == NUM_BUCKETS - 1) {  // AI wins
                printf(FG_BR(M)"\nI solved it in %d guesses!! ^.^\n"RESET, guess_idx);
                break;
            }

            // Make a guess based on current_list
            strcpy(guesses[guess_idx].word, guess_idx == 0 ? BEST_WORD : best_word(current_list, all_words, solutions));

            while (play_game) {
                printf("\nGuess #%2d:  "FG_BR(C)"%c   %c   %c   %c   %c\n"RESET, guess_idx + 1, guesses[guess_idx].word[0], guesses[guess_idx].word[1], guesses[guess_idx].word[2], guesses[guess_idx].word[3], guesses[guess_idx].word[4]);
                fputs("\nPlease provide 5-character color feedback (B = 0, Y = 1, G = 2): ", stdout);
                GETLINE(inp, 50);
                uint32_t col_code = 0;
                int valid_chars = 0;
                for (char *i = inp; *i != '\0'; i++) {
                    if (isspace(*i)) continue;
                    char c = TO_LOWER(*i);
                    valid_chars++;
                    if (c == 'l' && valid_chars == 1) {
                        for (char *j = i + 1; *j != '\0'; j++) if (!isspace(*j)) goto bad_input;
                        fputs(RESET FG_BR(G), stdout);
                        FOR_ITEM_IN_SET(word, current_list, printf("%s  ", word); );
                        fputs(RESET"\n", stdout);
                        goto next_input;
                    }
                    else if (c == 'q' && valid_chars == 1) {
                        for (char *j = i + 1; *j != '\0'; j++) if (!isspace(*j)) goto bad_input;
                        puts(FG_BR(Y)"\nThank you, I had fun!! :D\n");
                        play_game = 0;
                        goto quit_game;
                    }
                    else if (c == '0' || c == 'b') col_code = 3 * col_code + 0;
                    else if (c == '1' || c == 'y') col_code = 3 * col_code + 1;
                    else if (c == '2' || c == 'g') col_code = 3 * col_code + 2;
                    else goto bad_input;
                    if (valid_chars > WORD_LENGTH) goto bad_input;
                }
                if (valid_chars == 0) continue;
                if (valid_chars != WORD_LENGTH) goto bad_input;
                guesses[guess_idx].col_code = col_code;
                guesses[guess_idx].expect = sum_squares(guesses[guess_idx].word, current_list) / current_list->size;
                filter_list(guesses[guess_idx].word, current_list, col_code);
                guesses[guess_idx].num_poss = current_list->size;
                fputs("\n", stdout);
                guess_idx++;
                break;

                bad_input: puts(FG_BR(R)"Invalid feedback! "RESET"Please enter exactly 5 characters, made up of: "FG_BR(K)"B"RESET", "FG_BR(Y)"Y"RESET", "FG_BR(G)"G"RESET", "FG_BR(K)"0"RESET", "FG_BR(Y)"1"RESET" or "FG_BR(G)"2"RESET".\n"
                                "For example, if you're thinking of LUNAR and I guess SOLAR, you would enter "FG_BR(K)"BB"FG_BR(Y)"Y"FG_BR(G)"GG"RESET" or "FG_BR(K)"00"FG_BR(Y)"1"FG_BR(G)"22"RESET".\n"
                                INSTRUCTIONS_2);
                next_input:
            }
        }
        quit_game:
        set_free(current_list);
    }
};

void show_guess_sequence(hashset *all_words, hashset *solutions) {
    putchar('\n');
    while (1) {
        puts(FG_BR(Y)"Enter a Wordle solution word to see how I would guess it! "FG_BR(B)"Q"FG_BR(Y)" to quit.\n"RESET);
        char solution[WORD_LENGTH + 1] = {'\0'};
        prompt:
        fputs("-> ", stdout);
        GETLINE(inp, 50);
        int valid_chars = 0;
        for (char *i = inp; *i != '\0'; i++) {
            if (isspace(*i)) continue;
            if (!isalpha(*i) || valid_chars == 5) goto bad_input;
            solution[valid_chars++] = TO_UPPER(*i);
        }
        if (valid_chars == 0) goto prompt;
        if (valid_chars == 1 && solution[0] == 'Q') { putchar('\n'); return; }
        if (valid_chars != 5 || !set_has(solution, solutions, 0)) goto bad_input;
        char *guess = NULL;
        hashset *current_list = set_copy(solutions);
        for (int i = 1; guess == NULL || strcmp(guess, solution) != 0; i++) {
            if (guess == NULL) guess = BEST_WORD; else guess = best_word(current_list, all_words, solutions);
            double expect = sum_squares(guess, current_list) / current_list->size;
            uint32_t col_code = color_code(guess, solution);
            filter_list(guess, current_list, col_code);
            printf("Guess #%2d: ", i);
            print_color(guess, col_code);
            if (col_code != NUM_BUCKETS - 1)
                printf("(%d possibilities) "FG_BR(M)"[E(X) = %.3f]\n"RESET, current_list->size, expect);
            else
                puts(FG_BR(G)"- SOLUTION! -\n"RESET);
        }
        set_free(current_list);
        continue;
        bad_input: puts(FG_BR(R)"Not a valid Wordle solution! "RESET"See "FG_BR(B)"'wordle_solutions.txt'"RESET" for a list of solution words.");
    }
}

void show_AI_stats(char *filename, hashset *all_words, hashset *solutions) {
    putchar('\n');
    // Read previously computed word sequences from file.
    int total_guesses = 0, total_words = 0;
    char line[WORD_LENGTH * 10 + 1];
    FILE *guess_seqs = fopen(filename, "a+");
    if (guess_seqs != NULL) {
        char *last_word;
        while (fgets(line, WORD_LENGTH * 10, guess_seqs)) {
            int is_word = 0;
            for (char *s = strtok(line, " "); s != NULL; s = strtok(NULL, " ")) {
                total_guesses++;
                last_word = s;
                is_word = 1;
            }
            total_words += is_word;
        }
        if (total_guesses) printf("Last reached: '"FG_BR(G)"%.5s"RESET"'\nWords: %d   Guesses: %d\n\n", last_word, total_words, total_guesses);
    }

    int skip_words = total_words;
    FOR_ITEM_IN_SET(solution, solutions,
        if (skip_words) { skip_words--; continue; }
        total_words++;
        hashset *current_list = set_copy(solutions);
        char *guess = line;
        sprintf(guess, "%s", BEST_WORD);
        uint32_t guess_idx = 1;
        for ( ; ; guess_idx++) {
            printf(guess_idx > 1 ? RESET" -> "FG_BR(G)"%s"RESET : FG_BR(G)"%s"RESET, guess);
            uint32_t col_code = color_code(guess, solution);
            if (col_code == NUM_BUCKETS - 1) break;
            filter_list(guess, current_list, col_code);
            guess += WORD_LENGTH;
            sprintf(guess++, " %s", best_word(current_list, all_words, solutions));
        }
        total_guesses += guess_idx;
        fprintf(guess_seqs, "%s\n", line);
        putchar('\n');
        set_free(current_list);
        fflush(guess_seqs);
    );

    printf(FG_BR(M)"Done calculating all "FG_BR(W)"%d"FG_BR(M)" words.\nTotal guesses required: "FG_BR(W)"%d\n"FG_BR(M), total_words, total_guesses);
    puts("You may view guess-sequences for all words in "FG_BR(B)"'wordle_guess_seqs.txt'\n"FG_BR(M));
    double mean_guesses = (double) total_guesses / total_words;
    printf("Mean number of guesses required: "FG_BR(Y)"%.3f\n"FG_BR(M), mean_guesses);
    printf("Compared to MIT's (3.421) = "FG_BR(Y)"%.3f%%"FG_BR(M)" worse\n\n", 100 * (mean_guesses / 3.421 - 1));
}

int main(void) {

    srand(time(NULL));
    hashset *all_words = new_set(32768, 0, hash);
    load_words("wordle_guessables.txt", all_words);
    hashset *solutions = new_set(4096, 0, hash);
    load_words("wordle_solutions.txt", solutions);
    putchar ('\n');
    char *best_starter = "roate";

    while(1) {
        puts(FG_BR(B)"MMM's Wordle AI v1.2.0"RESET" by "FG(G)"max-min-median"RESET);
        puts("----------------------------------------");
        puts(FG_BR(Y)"[1]"RESET" Play Wordle!");
        puts(FG_BR(Y)"[2]"RESET" Let the AI guess your word");
        puts(FG_BR(Y)"[3]"RESET" Inspect AI's guess-sequence for a word");
        puts(FG_BR(Y)"[4]"RESET" See this AI's stats");
        puts(FG_BR(Y)"[Q]"RESET" Quit\n");
        
        while (1) {      
            fputs("Select an option: ", stdout);
            
            char inp[4];
            fgets(inp, 3, stdin);
            if (inp[1] != '\n') { FLUSH_STDIN; continue; }
            
            inp[0] = TO_LOWER(inp[0]);
            if (inp[0] == '1') play_Wordle(all_words, solutions);
            else if (inp[0] == '2') AI_play(all_words, solutions);
            else if (inp[0] == '3') show_guess_sequence(all_words, solutions);
            else if (inp[0] == '4') show_AI_stats("wordle_guess_seqs.txt", all_words, solutions);
            else if (inp[0] == 'q') return 0;
            else continue;
            break;
        }
    }
}