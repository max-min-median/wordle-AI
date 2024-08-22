# wordle-AI

An AI coded in `Node.js` to solve the word game [Wordle](https://www.nytimes.com/games/wordle/index.html), which exploded in popularity in 2022.

This solver is (only) 3.5% worse than [MIT's](https://auction-upload-files.s3.amazonaws.com/Wordle_Paper_Final.pdf) solver, a playable version of which can be found [here](http://wordle-page.s3-website-us-east-1.amazonaws.com/).

## Instructions

Clone this repository, then run
```
node wordle.js
```

## Dependencies
None

## How it works
As Wordle has been proven to be NP-hard, a peasant (like me) without massive computational resources has to plan carefully to even have a chance of a feasible algorithm which won't take 5 years to run. Therefore I chose a statistical / probabilistic approach, rather than exact game tree search algorithms.

There are 12-14K 'guessable' words (see `guessable.txt`) but only 2,315 solution words (see `answers.txt`).

Given a 'current word list' (a sublist of the 2,315 solution words), and the list of all guessables,
1. For each of the 14K guessable words,
2. For each word in the current word list, find the color coding that would result by guessing that guessable.
3. There are 238 'buckets', representing all possible color codings (3 possible colors per slot for 5 slots, however we subtract 5 since we cannot get permutations of 4 Greens and 1 Yellow).
4. We place this solution word into the respective bucket.
5. Now compute the expected size of our new sublist if we were to guess this word.
 - test
6. asdf
7. dsf
   

