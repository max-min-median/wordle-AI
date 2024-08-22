const { readFile } = require('./readFile');
const { appendFile } = require('./writeFile');
const { getInput } = require('./getInput');
const allWords = new Map();
const allWordsArray = [];
readFile("guessables.txt").forEach((x, i) => {allWords.set(x.toUpperCase(), i); allWordsArray[i] = x.toUpperCase()});
const answers = new Map();
const currList = [];
readFile("answers.txt").forEach((x, i) => {answers.set(x.toUpperCase(), allWords.get(x.toUpperCase())); currList.push(x.toUpperCase()); });
// const colorCodings = new Map();

const color = {
    brightwhite: w => `\x1b[1m${w}\x1b[0m`,
    green: w => `\x1b[1;42m${w}\x1b[0m`,
    yellow: w => `\x1b[1;43m${w}\x1b[0m`,
    red: w => `\x1b[31m${w}\x1b[0m`,
    brightred: w => `\x1b[91m${w}\x1b[0m`,
    magenta: w => `\x1b[1;95m${w}\x1b[0m`,
    lightblue: w => `\x1b[38;5;27m${w}\x1b[0m`,
};

(async function main() {
    while (true) {
        console.log(`${color.lightblue("MMM's Wordle AI v1.0.0")} by ${color.magenta("max-min-median")}`);
        console.log(`──────────────────────────────────────────`);
        console.log(`${color.brightwhite('[1]')} Play Wordle! (under development!)`);
        console.log(`${color.brightwhite('[2]')} Let the AI guess your word`);
        console.log(`${color.brightwhite('[3]')} Inspect AI's guess-sequence for a word`);
        console.log(`${color.brightwhite('[4]')} See this AI's stats`);
        console.log(`${color.brightwhite('[Q]')} Quit\n`);
        const selection = (await getInput(`Select an option: `)).toUpperCase().trim();
        switch (selection) {
            case '1': console.log(color.brightred('\nSorry, this feature is still under development!\n(I know, how silly is it to not be able to play Wordle on a Wordle app...)\n')); break;
            case '2': await AIPlay(); break;
            case '3':
                while (true) {
                    const word = (await getInput(`Enter a word to see how I would guess it (${color.lightblue('Q')} to quit): `)).toUpperCase().trim();
                    if (answers.has(word)) {
                        console.log(guessesForWord(word));
                    } else if (word === 'Q') {
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

async function AIPlay() {
    console.log(`\nLet's play Wordle! I'll guess and you tell me the colors! :)`);
    while (true) {
        let myList = currList.slice();
        const guesses = [];
        while (true) {
            if (myList.length === 0) {
                console.log(color.brightred("No words found that match above guesses! :(\nStarting a new game..."));
                break;
            }
            guesses.push([guesses.length === 0 ? 'ROATE' : bestWord(myList)]);
            console.log();
            guesses.slice(0, -1).forEach(([guess, colorCode, poss], i) => console.log(`Guess #${i+1}: ${colorize(guess, colorCode)}  (${poss} possibilities)`));
            if (guesses.at(-2)?.[1] === 242) {
                console.log(color.magenta(`\nI won in ${guesses.length - 1} guesses!! ^.^`));
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
                    console.log(`${color.lightblue('Thank you, I had fun!! :D\n')}`);
                    return;
                } else {
                    console.log(`${color.brightred('Invalid feedback, please enter exactly 5 characters, made up of: B, Y, G, 0, 1 or 2. ')}${color.lightblue('Q')}${color.brightred(' to quit.')}`);
                }
            }
        }
    }
};


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
    console.log(`Done calculating all words! You may view guess-sequences for all words in ${color.lightblue('all_guesses.txt')}.\n`);
    console.log("Mean number of guesses required =", totalGuesses / allGuessesList.length);
    console.log("Compared to MIT's (3.421) =", totalGuesses / allGuessesList.length / 3.421, "\n");
}


/**
 * Returns the sequence of guesses that this AI would eventually go through to guess `word`.
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
    let best = undefined, minExpectation = Infinity;
    const tryList = [];
    for (const word of allWordsArray) {
        const expectation = expectedBucketSize(word, currList);
        if (expectation < minExpectation) best = word, minExpectation = expectation;
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
    return sumSq / currList.length * (answers.has(guess) ? ((currList.length - 1) / currList.length) : 1);
}

function colorize(guess, colorCode) {
    const colorStr = colorCode.toString(3).padStart(5, '0');
    return [...guess].map(x => ` ${x} `).map((ch, i) => [x => x, color.yellow, color.green][+colorStr[i]](ch)).join(' ');
}