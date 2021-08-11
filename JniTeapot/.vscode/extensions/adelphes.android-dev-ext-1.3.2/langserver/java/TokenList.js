/**
 * @typedef {import('./tokenizer').Token} Token
 */
const ParseProblem = require('./parsetypes/parse-problem');

/**
 * 
 * @param {TokenList} tokens 
 * @param {ParseProblem} problem 
 */
function addproblem(tokens, problem) {
    tokens.problems.push(problem);
}

class TokenList {
    /**
     * @param {Token[]} tokens
     */
    constructor(tokens) {
        this.tokens = tokens;
        this.idx = -1;
        /** @type {Token} */
        this.current = null;
        this.inc();
        /** @type {ParseProblem[]} */
        this.problems = [];
        this.marks = [];
        this.last_mlc = '';
    }

    /**
     * Returns and consumes the current token
     */
    consume() {
        const tok = this.current;
        this.inc();
        return tok;
    }

    inc() {
        for (; ;) {
            this.current = this.tokens[this.idx += 1];
            if (!this.current || this.current.kind !== 'wsc') {
                return this.current;
            }
            const wsc = this.current.value;
            if (wsc.startsWith('/*')) {
                this.last_mlc = wsc;
            }
        }
    }

    clearMLC() {
        this.last_mlc = '';
    }

    getLastMLC() {
        const s = this.last_mlc;
        this.last_mlc = '';
        return s;
    }
    
    mark() {
        this.marks.unshift(this.idx);
    }

    /**
     * Returns the array of tokens from the last mark() point, trimming any trailing whitespace tokens
     */
    markEnd() {
        let i = this.idx;
        while (this.tokens[--i].kind === 'wsc') { }
        const range = [this.marks.shift(), i + 1];
        if (range[1] <= range[0]) {
            range[1] = range[0] + 1;
        }
        return this.tokens.slice(range[0], range[1]);
    }

    /**
     * Token lookahead. The current token is unaffected by this method.
     * @param {number} n number of tokens to look ahead
     */
    peek(n) {
        let token, idx = this.idx;
        while (--n >= 0) {
            for (; ;) {
                token = this.tokens[idx += 1];
                if (!token || token.kind !== 'wsc') {
                    break;
                }
            }
        }
        return token;
    }

    /**
     * Check if the current token matches the specified kind, returns and consumes it
     * @param {string} kind
     */
    getIfKind(kind) {
        const token = this.current;
        if (token && token.kind === kind) {
            this.inc();
            return token;
        }
        return null;
    }

    /**
     * Check if the current token matches the specified value, returns and consumes it
     * @param {string} value
     */
    getIfValue(value) {
        const token = this.current;
        if (token && token.value === value) {
            this.inc();
            return token;
        }
        return null;
    }

    /**
     * Check if the current token matches the specified value and consumes it
     * @param {string} value
     */
    isValue(value) {
        return this.getIfValue(value) !== null;
    }

    /**
     * Check if the current token matches the specified kind and consumes it
     * @param {string} kind
     */
    isKind(kind) {
        return this.getIfKind(kind) !== null;
    }

    /**
     * Check if the current token matches the specified value and consumes it or reports an error
     * @param {string} value
     */
    expectValue(value) {
        if (this.isValue(value)) {
            return true;
        }
        const token = this.current || this.tokens[this.tokens.length - 1];
        addproblem(this, ParseProblem.Error(token, `${value} expected`));
        return false;
    }

    get previous() {
        for (let idx = this.idx - 1; idx >= 0; idx--) {
            if (idx <= 0) {
                return this.tokens[0];
            }
            if (this.tokens[idx].kind !== 'wsc') {
                return this.tokens[idx];
            }
        }
    }

    /**
     * @param {number} start 
     * @param {number} delete_count 
     * @param  {...Token} insert 
     */
    splice(start, delete_count, ...insert) {
        this.tokens.splice(start, delete_count, ...insert);
        this.current = this.tokens[this.idx];
    }
}

exports.TokenList = TokenList;
exports.addproblem = addproblem;
