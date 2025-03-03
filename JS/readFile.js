function readFile(filename) {  
    const fs = require('node:fs');
    try {
        return fs.readFileSync(filename, 'utf8').trimEnd().split(/\r?\n\r?/);
    } catch (err) {
        console.error(err);
        return err;
    }
}

module.exports = { readFile };