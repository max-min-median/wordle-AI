async function getInput(prompt='?') {
    const readlinePromises = require('node:readline/promises');
    const rl = readlinePromises.createInterface({
        input: process.stdin,
        output: process.stdout,
    });
    const answer = await rl.question(prompt);
    rl.close();
    return answer;
}

module.exports = { getInput };