const { readFile } = require('./readFile');
const { appendFile } = require('./writeFile');
const { getInput } = require('./getInput');
const allWords = new Map();
const allWordsArray = [];
readFile("wordle_guessables.txt").forEach((x, i) => {allWords.set(x, i); allWordsArray[i] = x});
const answers = new Map();
const currList = [];
readFile("wordle_solutions.txt").forEach((x, i) => {answers.set(x, allWords.get(x)); currList.push(x); });
// const colorCodings = new Map();

const color = {
    brightwhite: w => `\x1b[1m${w}\x1b[0m`,
    greenBG: w => `\x1b[1;42m${w}\x1b[0m`,
    yellowBG: w => `\x1b[1;43m${w}\x1b[0m`,
    red: w => `\x1b[31m${w}\x1b[0m`,
    yellow: w => `\x1b[33m${w}\x1b[0m`,
    green: w => `\x1b[92m${w}\x1b[0m`,    
    brightred: w => `\x1b[91m${w}\x1b[0m`,
    gray: w => `\x1b[90m${w}\x1b[0m`,
    magenta: w => `\x1b[1;95m${w}\x1b[0m`,
    lightblue: w => `\x1b[38;5;27m${w}\x1b[0m`,
};


/**
 * Main menu
 */
(async function main() {
    while (true) {
        console.log(`${color.lightblue("MMM's Wordle AI v1.0.0")} by ${color.green("max-min-median")}`);
        console.log(`──────────────────────────────────────────`);
        console.log(`${color.brightwhite('[1]')} Play Wordle!`);
        console.log(`${color.brightwhite('[2]')} Let the AI guess your word`);
        console.log(`${color.brightwhite('[3]')} Inspect AI's guess-sequence for a word`);
        console.log(`${color.brightwhite('[4]')} See this AI's stats`);
        console.log(`${color.brightwhite('[Q]')} Quit\n`);
        const selection = (await getInput(`Select an option: `)).toUpperCase().trim();
        switch (selection) {
            case '1': await playWordle(); break; // console.log(color.brightred('\nSorry, this feature is still under development!\n(I know, how silly is it to not be able to play Wordle on a Wordle app...)\n')); break;
            case '2': await AIPlay(); break;
            case '3':
                while (true) {
                    const word = (await getInput(`Enter a word to see how I would guess it (${color.lightblue('Q')} to quit): `)).toUpperCase().trim();
                    if (answers.has(word)) {
                        console.log(guessesForWord(word));
                    } else if (word === 'Q') {
                        console.log();
                        break;
                    } else {
                        console.log(color.brightred("Not a valid Wordle solution!\n"))
                    }
                }
                break;
            case '4': computeMeanGuesses(); break;
            case 'Q': return;
        }
    }
})();


async function playWordle() {
    console.log(`${color.yellow("\nLet's play Wordle! Try to guess a 5-letter word! :) ")}${color.lightblue('Q')}${color.yellow(" to quit")}`);
    while (true) {
        const guesses = [];
        let myList = currList.slice();
        const solution = [...answers.keys()][Math.floor(Math.random() * answers.size) + 1];
        // console.log(`The secret answer is ${solution}!`);
        while (true) {
            console.log();
            guesses.forEach(([guess, colorCode, poss, exp, best, bestExp], i) => console.log(`Guess #${i+1}: ${colorize(guess, colorCode)} (${poss} possibilities) ${color.magenta(`[E(X) = ${exp.toFixed(3)}]`)}   ${color.lightblue(`best: ${best} (E(X) = ${bestExp.toFixed(3)})`)}`));
            if (guesses.at(-1)?.[0] === solution) {
                console.log(color.magenta(`\nYou solved it in ${guesses.length} guesses! Well done :D\n`));
                break;
            }
            const guess = (await getInput(`Guess #${guesses.length + 1}:  `)).toUpperCase().replaceAll(' ', '');
            if (guess === 'Q' || guess === 'G') {            
                console.log(`${color.yellow("\nThe answer was: ")}${color.magenta([...solution].map(x => ` ${x} `).join(''))}${color.yellow(" >:)")}`);
                if (guess === 'Q') {
                    console.log(`${color.yellow('Bye, hope you had fun!! ^o^\n')}`);
                    return;
                }
            } else if (guess === 'L') {
                console.log('\n', color.green(myList.join('  ')));
            } else if (allWords.has(guess)) {
                const colCode = colorCode(guess, solution);
                const expectation = expectedBucketSize(guess, myList);
                const bestGuess = (guesses.length === 0 ? 'ROATE' : bestWord(myList)), bestExpect = expectedBucketSize(bestGuess, myList);
                guesses.push([guess, colCode, (myList = filteredList(guess, myList, colCode)).length, expectation, bestGuess, bestExpect]);
            } else {
                console.log(`${color.brightred("Not a valid guess! See ")}${color.lightblue('all_guesses.txt')}${color.brightred(" for a list of guessable words.")}`);
                console.log(`${color.lightblue("G")}${color.brightred(" to give up this round")}, ${color.lightblue("L")}${color.brightred(" to peek at the list of possible words")}, ${color.lightblue("Q")}${color.brightred(" to quit to main menu.")}`);
                continue;
            }
        }
    }
}



/**
 * 'Plays' Wordle by trying to guess a word only known to the user. Expects input from the user as to the color-coded feedback of each guess.
 * The feedback must be provided as a 5-character string with only these characters: `B, Y, G` or the corresponding numbers `0, 1, 2`.
 */
async function AIPlay() {
    console.log(`${color.yellow("\nLet's play Wordle! I'll guess and you tell me the colors! :)")}`);
    while (true) {
        let myList = currList.slice();
        const guesses = [];
        while (true) {
            if (myList.length === 0) {
                console.log(color.brightred("No words found that match above guesses! :(\nStarting a new game..."));
                break;
            }
            guesses.push([guesses.length === 0 ? 'ROATE' : bestWord(myList)]);
            if (guesses.length > 1) console.log();
            guesses.slice(0, -1).forEach(([guess, colorCode, poss], i) => console.log(`Guess #${i+1}: ${colorize(guess, colorCode)}  (${poss} possibilities)`));
            if (guesses.at(-2)?.[1] === 242) {
                console.log(color.lightblue(`\nI won in ${guesses.length - 1} guesses!! ^.^`));
                break;
            }
            while (true) {
                console.log(`\nGuess #${guesses.length}: ${color.magenta([...guesses.at(-1)[0]].map(x => ` ${x} `).join(' '))}`);
                const feedback = (await getInput("Please provide 5-character color feedback (B = 0, Y = 1, G = 2): ")).toUpperCase();
                if (feedback.match(/^[GYB0-2]{5}$/)) {
                    const colCode = parseInt(feedback.replaceAll('G', '2').replaceAll('Y', '1').replaceAll('B', '0'), 3);
                    guesses.at(-1).push(colCode);
                    myList = filteredList(guesses.at(-1)[0], myList, colCode);
                    guesses.at(-1).push(myList.length);
                    break;
                } else if (feedback === "Q") {
                    console.log(`${color.yellow('\nThank you, I had fun!! :D\n')}`);
                    return;
                } else {
                    console.log(`${color.brightred('Invalid feedback, please enter exactly 5 characters, made up of: B, Y, G, 0, 1 or 2. ')}`);
                    console.log(`${color.brightred("For example, if you're thinking of LUNAR and I guess SOLAR, you would enter ")}${color.gray("00")}${color.yellow("1")}${color.green("22")}${color.brightred(" or ")}${color.gray("BB")}${color.yellow("Y")}${color.green("GG")}`);
                    console.log(`${color.brightred("Type ")}${color.lightblue('Q')}${color.brightred(' to quit.')}`);
                }
            }
        }
    }
};


/**
 * Calls `guessesForWord` on each of the 2315 words in the solution list. Then calculates the mean number of guesses required per word.
 * Reads from and writes sequences of guesses to `all_guesses.txt`. Deleting this file will cause this function perform a recalculation.
 */
function computeMeanGuesses() {
    console.log();
    let allGuessesList = readFile('all_guesses.txt');
    if (allGuessesList instanceof Error) allGuessesList = []; else allGuessesList = allGuessesList.map(x => x.split(','));
    let totalGuesses = allGuessesList.flat().length;
    const solutionsToDo = [...answers.keys()].slice(allGuessesList.length);
    for (const solution of solutionsToDo) {
        const guesses = guessesForWord(solution);
        allGuessesList.push(guesses);
        totalGuesses += guesses.length;
        console.log(guesses, totalGuesses / allGuessesList.length);
        appendFile('all_guesses.txt', guesses.join(',') + '\n');
    }
    console.log(`${color.magenta(`Done calculating all words!`)} You may view guess-sequences for all words in ${color.lightblue('all_guesses.txt')}.\n`);
    console.log("Mean number of guesses required =", color.yellow((totalGuesses / allGuessesList.length).toFixed(3)));
    console.log("Compared to MIT's (3.421) =", color.yellow((totalGuesses / allGuessesList.length / 3.421).toFixed(3)), "\n");
}

/**
 * Returns the sequence of guesses that this AI would eventually use to guess `word`.
 */
function guessesForWord(word) {
    let myList = filteredList('ROATE', currList.slice(), colorCode('ROATE', word));
    const guesses = ['ROATE'];
    while (guesses.at(-1) !== word) {
        const guess = bestWord(myList);
        guesses.push(guess);
        myList = filteredList(guess, myList, colorCode(guess, word));
    }
    return guesses;
}

/**
 * Returns a new (sub-)list of words from `currList` which satisfy the `colorCode` obtained by guessing the word `guess`.
 */
function filteredList(guess, currList, colCode) { return currList.filter(word => colorCode(guess, word) === colCode); }

/**
 * Returns the word which minimizes the expected length of the new list of words obtained by guessing it.
 */
function bestWord(currList) {
    if (currList.length <= 2) return currList[0];
    let best = undefined, minExpectation = Infinity, bestIsASolution = false;
    const tryList = [];
    for (const word of allWordsArray) {
        const expectation = expectedBucketSize(word, currList);
        if (expectation < minExpectation || (expectation === minExpectation && !bestIsASolution && answers.has(word)))
            best = word, minExpectation = expectation, bestIsASolution = answers.has(word);
            // console.log(`Found new best word: ${best} (Expectation: ${minExpectation})`);
        // tryList.push([word, expectation]);
    }
    // tryList.sort((a, b) => a[1] - b[1]);
    // write("words_ranked.txt", tryList.map(x => x.join(",")).join("\n"));
    return best;
}


/** 
 * Returns a color-coding as a ternary number with 0 = black, 1 = yellow, 2 = green.
 * @example If solution is LOGIC, guessing LIMBO returns 190 (dec) = 21001 (ternary).
 * @param {String} guess - a five-letter 'guess' string
 * @param {String} solution - a five-letter string to match the guess against
 */
function colorCode(guess, solution) {
    // let codingIndex = ((typeof guess === 'number' ? guess : allWords.get(guess)) << 16) + (typeof solution === 'number' ? solution : allWords.get(solution));
    // let result = colorCodings.get(codingIndex);
    // if (result !== undefined) {
    //     console.log(`Found repeat request: ${result}`)
    //     return result;
    // }
    // result = 0;
    let result = 0;

    if (typeof guess === 'number') guess = allWordsArray[guess];
    if (typeof solution === 'number') solution = allWordsArray[solution];
    const wordLength = 5;
    solution = [...solution], guess = [...guess];
    // let resultString = [...'.....'];

    for (let i = 0; i < wordLength; i++) {
        if (guess[i] === solution[i]) {
            // resultString[i] = 'G';
            result += 2 * 3 ** (wordLength - i - 1);
            guess[i] = solution[i] = '';
        }
    }

    for (let i = 0; i < wordLength; i++) {
        if (guess[i] === '') continue;
        for (let j = 0; j < wordLength; j++) {
            if (guess[i] === solution[j]) {
                // resultString[i] = 'Y';
                result += 3 ** (wordLength - i - 1);
                solution[j] = '';
                break;
            }
        }
    }
    // colorCodings.set(codingIndex, result);
    return result;
}

/**
 * Returns the expected size of the new list of words obtained by guessing `guess`, assuming that the solution is a
 * uniformly random word in `currList`.
 */
function expectedBucketSize(guess, currList) {
    const buckets = new Map();
    for (const word of currList) {
        const bucket = colorCode(guess, word);
        buckets.set(bucket, (buckets.get(bucket) ?? 0) + 1);
    }
    let sumSq = 0;
    for (const bucketSize of buckets.values()) {
        sumSq += bucketSize ** 2;
    }
    return sumSq / currList.length;
}

function colorize(guess, colorCode) {
    const colorStr = colorCode.toString(3).padStart(5, '0');
    return [...guess].map(x => ` ${x} `).map((ch, i) => [x => x, color.yellowBG, color.greenBG][+colorStr[i]](ch)).join(' ');
}