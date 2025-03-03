function writeFile(filename, data) {
    const fs = require('node:fs');
    try {
        fs.writeFileSync(filename, data);
    } catch (err) {
        console.error(err);
    }
}

function appendFile(filename, data) {
    const fs = require('node:fs');
    try {
        fs.appendFileSync(filename, data);
    } catch (err) {
        console.error(err);
    }
}

module.exports = { writeFile, appendFile };