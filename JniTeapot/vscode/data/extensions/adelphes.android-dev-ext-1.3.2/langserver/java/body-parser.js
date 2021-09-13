/**
 * Method body parsing is entirely linear and relies upon type processing being completed so
 * we can resolve packages, types, fields, methods, parameters and locals along the way.
 * 
 * Each token also contains detailed state information used for completion suggestions.
 */
const { CEIType, PrimitiveType, ArrayType, UnresolvedType, TypeVariable, Field, Method } = require('java-mti');
const { SourceType, SourceTypeIdent, SourceField, SourceMethod, SourceConstructor, SourceInitialiser, SourceParameter, SourceAnnotation,
    SourceUnit, SourcePackage, SourceImport, SourceArrayType, FixedLengthArrayType, NamedSourceType, AnonymousSourceType } = require('./source-types');
const ResolvedImport = require('./parsetypes/resolved-import');
const ParseProblem = require('./parsetypes/parse-problem');
const { tokenize, Token } = require('./tokenizer');
const { resolveTypeOrPackage, resolveNextTypeOrPackage } = require('./type-resolver');
const { genericTypeArgs, typeIdent, typeIdentList } = require('./typeident');
const { TokenList, addproblem } = require("./TokenList");
const { AnyMethod, AnyType, AnyValue } = require("./anys");
const { Label, Local, MethodDeclarations, ResolvedIdent } = require("./body-types");
const { resolveImports, resolveSingleImport } = require('./import-resolver');
const { getTypeInheritanceList } = require('./expression-resolver');
const { checkStatementBlock } = require('./statement-validater');
const { time, timeEnd } = require('../logging');

const { ArrayIndexExpression } = require("./expressiontypes/ArrayIndexExpression");
const { ArrayValueExpression } = require("./expressiontypes/ArrayValueExpression");
const { BinaryOpExpression } = require("./expressiontypes/BinaryOpExpression");
const { BracketedExpression } = require("./expressiontypes/BracketedExpression");
const { CastExpression } = require("./expressiontypes/CastExpression");
const { IncDecExpression } = require("./expressiontypes/IncDecExpression");
const { LambdaExpression } = require("./expressiontypes/LambdaExpression");
const { MemberExpression } = require("./expressiontypes/MemberExpression");
const { MethodCallExpression } = require("./expressiontypes/MethodCallExpression");
const { NewArray, NewObject } = require("./expressiontypes/NewExpression");
const { TernaryOpExpression } = require("./expressiontypes/TernaryOpExpression");
const { UnaryOpExpression } = require("./expressiontypes/UnaryOpExpression");
const { Variable } = require("./expressiontypes/Variable");

const { BooleanLiteral } = require('./expressiontypes/literals/Boolean');
const { CharacterLiteral } = require('./expressiontypes/literals/Character');
const { InstanceLiteral } = require('./expressiontypes/literals/Instance');
const { NumberLiteral } = require('./expressiontypes/literals/Number');
const { NullLiteral } = require('./expressiontypes/literals/Null');
const { StringLiteral } = require('./expressiontypes/literals/String');

const { AssertStatement } = require("./statementtypes/AssertStatement");
const { Block } = require("./statementtypes/Block");
const { BreakStatement } = require("./statementtypes/BreakStatement");
const { ContinueStatement } = require("./statementtypes/ContinueStatement");
const { DoStatement } = require("./statementtypes/DoStatement");
const { EmptyStatement } = require("./statementtypes/EmptyStatement");
const { ExpressionStatement } = require("./statementtypes/ExpressionStatement");
const { ForStatement } = require("./statementtypes/ForStatement");
const { IfStatement } = require("./statementtypes/IfStatement");
const { InvalidStatement } = require("./statementtypes/InvalidStatement");
const { LocalDeclStatement } = require("./statementtypes/LocalDeclStatement");
const { ReturnStatement } = require("./statementtypes/ReturnStatement");
const { Statement } = require("./statementtypes/Statement");
const { SwitchStatement } = require("./statementtypes/SwitchStatement");
const { SynchronizedStatement } = require("./statementtypes/SynchronizedStatement");
const { ThrowStatement } = require("./statementtypes/ThrowStatement");
const { TryStatement } = require("./statementtypes/TryStatement");
const { WhileStatement } = require("./statementtypes/WhileStatement");

/**
 * @typedef {import('./source-types').SourceMethodLike} SourceMethodLike
 * @typedef {SourceType|SourceMethodLike} Scope
 */


/**
 * @param {*[]} blocks 
 * @param {boolean} isMethod 
 */
function flattenBlocks(blocks, isMethod) {
    return blocks.reduce((arr,block) => {
        if (block instanceof Token) {
            // 'default' and 'synchronised' are not modifiers inside method bodies
            if (isMethod && block.kind === 'modifier' && /^(default|synchronized)$/.test(block.value)) {
                block.kind = 'statement-kw'
                block.simplified = block.value;
            }
            arr.push(block);
        } else {
            arr = [...arr, ...flattenBlocks(block.blockArray().blocks, isMethod)];
        }
        return arr;
    }, [])
}

/**
 * @param {SourceType} type 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap
 */
function parseTypeMethods(type, imports, typemap) {
    type.initers.forEach(i => {
        i.parsed = parseBody(i, imports, typemap);
    })
    type.constructors.forEach(c => {
        c.parsed = parseBody(c, imports, typemap);
    })
    type.sourceMethods.forEach(m => {
        m.parsed = parseBody(m, imports, typemap);
    })
}

/**
 * @param {SourceMethod | SourceConstructor | SourceInitialiser} method 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function parseBody(method, imports, typemap) {
    const body_tokens = method.body.tokens;
    if (!body_tokens || body_tokens[0].value !== '{') {
        return null;
    }
    const tokenlist = new TokenList(flattenBlocks(body_tokens, true));
    let mdecls = new MethodDeclarations();
    try {
        method.body.block = statementBlock(tokenlist, mdecls, method, imports, typemap);
        checkStatementBlock(method.body.block, method, typemap, tokenlist.problems);
    } catch (err) {
        addproblem(tokenlist, ParseProblem.Information(tokenlist.current, `Parse failed: ${err.message}`));
    }
    return {
        block: method.body.block,
        problems: tokenlist.problems,
    }
}

/**
 * @param {TokenList} tokens 
 * @param {*} typemap 
 */
function extractSourceTypes(tokens, typemap) {
    // first strip out any comments, chars, strings etc which might confuse the parsing
    const normalised_source = tokens.tokens.map(t => {
        return /wsc|string-literal|char-literal/.test(t.kind) 
          ? ' '.repeat(t.length)
          : t.source
    }).join('')
    
    // look for scope boundaries, package and type declarations
    const re = /(\{)|(\})|\bpackage +(\w+(?: *\. *\w+)*)|\b(class|enum|interface|@ *interface) +(\w+)/g;
    let package_name = null;
    let type_stack = [];
    let code_balance = 0;
    const source_types = [];
    function findTokenAt(idx) {
        return tokens.tokens.find(t => t.range.start === idx);        
    }
    for (let m; m = re.exec(normalised_source);) {
        if (code_balance) {
            if (m[1]) code_balance += 1;
            else if (m[2]) code_balance -= 1;
            continue;
        }
        if (m[1]) {
            // open brace
            if (!type_stack[0]) {
                continue;   // ignore - we haven't started a type yet
            }
            if (!type_stack[0].type_open) {
                type_stack[0].type_open = true; // start of type body
                continue;
            }
            // start of method body or array expression
            code_balance = 1;
        } else if (m[2]) {
            // close brace
            if (!type_stack[0]) {
                continue;   // we're outside any type
            }
            type_stack.shift();
        } else if (m[3]) {
            // package name
            if (package_name !== null) {
                continue;   // ignore - we already have a package name or started parsing types
            }
            package_name = m[3].replace(/ +/g, '');
        } else if (m[4]) {
            // named type decl
            package_name = package_name || '';
            const typeKind = m[4].replace(/ +/g, ''),
              kind_token = findTokenAt(m.index),
              name_token = findTokenAt(m.index + m[0].match(/\w+$/).index),
              outer_type = type_stack[0] && type_stack[0].source_type,
              source_type = new NamedSourceType(package_name, outer_type, '', [], typeKind, kind_token, name_token, typemap);
              
            type_stack.unshift({
                source_type,
                type_open: false,
            });
            source_types.unshift(source_type);
        }
    }
    return source_types;
}

/**
 * @param {{uri:string, content:string, version:number}[]} docs
 * @param {SourceUnit[]} cached_units
 * @param {Map<string,CEIType>} typemap 
 * @returns {SourceUnit[]}
 */
function parse(docs, cached_units, typemap) {

    time('tokenize');
    const sources = docs.reduce((arr, doc) => {
        try {
            const unit = new SourceUnit();
            unit.uri = doc.uri;
            const tokens = new TokenList(unit.tokens = tokenize(doc.content));
            arr.push({ unit, tokens });
        } catch(err) {
        }
        return arr;
    }, [])
    timeEnd('tokenize');

    // add the cached types to the type map
    cached_units.forEach(unit => {
        unit.types.forEach(t => typemap.set(t.shortSignature, t));
    })

    // in order to resolve types as we parse, we must extract the set of source types first
    sources.forEach(source => {
        const source_types = extractSourceTypes(source.tokens, typemap);
        // add them to the type map
        source_types.forEach(t => typemap.set(t.shortSignature, t));
    })

    // parse all the tokenized sources
    time('parse');
    sources.forEach(source => {
        try {
            parseUnit(source.tokens, source.unit, typemap);
            // once all the types have been parsed, resolve any field initialisers
            // const ri = new ResolveInfo(typemap, tokens.problems);
            // unit.types.forEach(t => {
            //     t.fields.filter(f => f.init).forEach(f => checkAssignment(ri, f.type, f.init));
            // });
        } catch (err) {
            addproblem(source.tokens, ParseProblem.Error(source.tokens.current, `Parse failed: ${err.message}`));
        }
    });
    timeEnd('parse');

    return sources.map(s => s.unit);
}

/**
 * @param {TokenList} tokens
 * @param {SourceUnit} unit
 * @param {Map<string,CEIType>} typemap 
 */
function parseUnit(tokens, unit, typemap) {
    let package_name = '';
    // init resolved imports with java.lang.*
    let resolved_imports = resolveImports(typemap, null).slice();
    // retrieve the implicit imports
    while (tokens.current) {
        let modifiers = [], annotations = [];
        for (;tokens.current;) {
            if (tokens.current.kind === 'modifier') {
                modifiers.push(tokens.current);
                tokens.inc();
                continue;
            }
            if (tokens.current.value === '@') {
                tokens.inc().value === 'interface'
                    ? sourceType(tokens.getLastMLC(), modifiers, tokens, package_name, '@interface', unit, resolved_imports, typemap)
                    : annotations.push(annotation(tokens, null, resolved_imports, typemap));
                continue;
            }
            break;
        }
        if (!tokens.current) {
            break;
        }
        switch (tokens.current.value) {
            case 'package':
                if (unit.package_ !== null) {
                    addproblem(tokens, ParseProblem.Error(tokens.current, `Multiple package declarations`));
                }
                if (modifiers[0]) {
                    addproblem(tokens, ParseProblem.Error(tokens.current, `Unexpected modifier: ${modifiers[0].source}`));
                }
                tokens.clearMLC();
                const pkg = packageDeclaration(tokens);
                if (!package_name) {
                    unit.package_ = pkg;
                    package_name = pkg.name;
                    const imprts = resolveImports(typemap, pkg.name, []);
                    if (imprts.length) {
                        resolved_imports.unshift(...imprts);
                    }
                }
                continue;
            case 'import':
                if (modifiers[0]) {
                    addproblem(tokens, ParseProblem.Error(tokens.current, `Unexpected modifier: ${modifiers[0].source}`));
                }
                tokens.clearMLC();
                const imprt = importDeclaration(tokens, typemap);
                unit.imports.push(imprt);
                if (imprt.resolved) {
                    resolved_imports.unshift(imprt.resolved);
                }
                continue;
        }
        if (tokens.current.kind === 'type-kw') {
            sourceType(tokens.getLastMLC(), modifiers, tokens, package_name, tokens.current.value, unit, resolved_imports, typemap);
            continue;
        }
        addproblem(tokens, ParseProblem.Error(tokens.current, 'Type declaration expected'));
        // skip until something we recognise
        while (tokens.current) {
            if (/@|package|import/.test(tokens.current.value) || /modifier|type-kw/.test(tokens.current.kind)) {
                break;
            }
            tokens.inc();
        }
    }
    return unit;
}

/**
 * @param {TokenList} tokens 
 */
function packageDeclaration(tokens) {
    tokens.mark();
    tokens.current.loc = { key:'pkgname' };
    tokens.expectValue('package');
    let pkg_name_parts = [], dot;
    for (;;) {
        let name = tokens.current;
        if (!tokens.isKind('ident')) {
            name = null;
            addproblem(tokens, ParseProblem.Error(tokens.current, `Package identifier expected`));
        }
        if (name) {
            name.loc = { key: `pkgname:${pkg_name_parts.join('/')}` };
            pkg_name_parts.push(name.value);
        }
        if (dot = tokens.getIfValue('.')) {
            dot.loc = { key :`pkgname:${pkg_name_parts.join('/')}` };
            continue;
        }
        const decl_tokens = tokens.markEnd();
        semicolon(tokens);
        return new SourcePackage(decl_tokens, pkg_name_parts.join('.'));
    }
}

/**
 * @param {TokenList} tokens 
 * @param {Map<string,CEIType>} typemap 
 */
function importDeclaration(tokens, typemap) {
    tokens.mark();
    tokens.current.loc = { key: 'fqdi:' };
    tokens.expectValue('import');
    const static_token = tokens.getIfValue('static');
    let asterisk_token = null, dot;
    const pkg_token_parts = [], pkg_name_parts = [];
    for (;;) {
        let name = tokens.current;
        if (!tokens.isKind('ident')) {
            name = null;
            addproblem(tokens, ParseProblem.Error(tokens.current, `Package identifier expected`));
        }
        if (name) {
            name.loc = { key: `fqdi:${pkg_name_parts.join('.')}` };
            pkg_token_parts.push(name);
            pkg_name_parts.push(name.value);
        }
        if (dot = tokens.getIfValue('.')) {
            dot.loc = name && name.loc;
            if (!(asterisk_token = tokens.getIfValue('*'))) {
                continue;
            }
        }
        const decl_tokens = tokens.markEnd();
        semicolon(tokens);

        const pkg_name = pkg_name_parts.join('.');
        const resolved = resolveSingleImport(typemap, pkg_name, !!static_token, !!asterisk_token, 'import');

        return new SourceImport(decl_tokens, pkg_token_parts, pkg_name, static_token, asterisk_token, resolved);
    }
}

/**
 * @param {SourceMethodLike} method
 * @param {MethodDeclarations} mdecls 
 * @param {Local[]} new_locals 
 */
function addLocals(method, mdecls, new_locals) {
    for (let local of new_locals) {
        mdecls.locals.unshift(local);
    }
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {SourceMethodLike} method 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 * @returns {Statement}
 */
function statement(tokens, mdecls, method, imports, typemap) {
    let s, modifiers = [];
    for (;;) {
        switch(tokens.current.kind) {
            case 'modifier':
                modifiers.push(tokens.current);
                tokens.inc();
                continue;
            case 'type-kw':
                sourceType('', modifiers.splice(0,1e9), tokens, method, tokens.current.value, mdecls, imports, typemap);
                continue;
        }
        break;
    }
    // modifiers are only allowed on local variable decls
    if (modifiers.length) {
        const type = typeIdent(tokens, method, imports, typemap);
        const locals = var_ident_list(modifiers, type, null, tokens, mdecls, method, imports, typemap)
        addLocals(method, mdecls, locals);
        semicolon(tokens);
        return new LocalDeclStatement(method, locals);
    }

    switch(tokens.current.kind) {
        case 'statement-kw':
            s = statementKeyword(tokens, mdecls, method, imports, typemap);
            return s;
        case 'ident':
            // checking every statement identifier for a possible label is really inefficient, but trying to
            // merge this into expression_or_var_decl is worse for now
            if (tokens.peek(1).value === ':') {
                const label = new Label(tokens.current);
                tokens.inc(), tokens.inc();
                // ignore and just return the next statement
                // - we cannot return the label as a statement because for/if/while check the next statement type
                // the labels should be collated and checked for duplicates, etc
                return statement(tokens, mdecls, method, imports, typemap);
            }
            // fall-through to expression_or_var_decl
        case 'primitive-type':
            const exp_or_vardecl = expression_or_var_decl(tokens, mdecls, method, imports, typemap);
            if (Array.isArray(exp_or_vardecl)) {
                addLocals(method, mdecls, exp_or_vardecl);
                s = new LocalDeclStatement(method, exp_or_vardecl);
            } else {
                s = new ExpressionStatement(method, exp_or_vardecl);
            }
            semicolon(tokens);
            return s;
        case 'string-literal':
        case 'char-literal':
        case 'number-literal':
        case 'boolean-literal':
        case 'object-literal':
        case 'inc-operator':
        case 'plumin-operator':
        case 'unary-operator':
        case 'open-bracket':
        case 'new-operator':
            const e = expression(tokens, mdecls, method, imports, typemap);
            s = new ExpressionStatement(method, e);
            semicolon(tokens);
            return s;
    }
    switch(tokens.current.value) {
        case ';':
            tokens.inc();
            return new EmptyStatement(method);
        case '{':
            return statementBlock(tokens, mdecls, method, imports, typemap);
        case '}':
            return new EmptyStatement(method);
    }
    return new InvalidStatement(method, tokens.consume());
}

/**
* @param {string} docs
* @param {Token[]} modifiers
* @param {TokenList} tokens 
* @param {Scope|string} scope_or_pkgname
* @param {string} typeKind
* @param {{types:SourceType[]}} owner
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function sourceType(docs, modifiers, tokens, scope_or_pkgname, typeKind, owner, imports, typemap) {
    let package_name, scope;
    if (typeof scope_or_pkgname === 'string') {
        package_name = scope_or_pkgname;
        scope = null;
    } else {
        const scoped_type = scope_or_pkgname instanceof SourceType ? scope_or_pkgname : scope_or_pkgname.owner;
        package_name = scoped_type.packageName;
        scope = scope_or_pkgname;
    }
    const type = typeDeclaration(package_name, scope, docs, modifiers, typeKind, tokens.current, tokens, imports, typemap);
    if (!type) {
        return;
    }
    owner.types.push(type);
    if (!(owner instanceof MethodDeclarations)) {
        typemap.set(type.shortSignature, type);
    }
    if (tokens.isValue('extends')) {
        type.extends_types = typeIdentList(tokens, type, imports, typemap);
    }
    if (tokens.isValue('implements')) {
        type.implements_types = typeIdentList(tokens, type, imports, typemap);
    }
    tokens.clearMLC();
    tokens.expectValue('{');
    if (type.typeKind === 'enum') {
        if (!/[;}]/.test(tokens.current.value)) {
            enumValueList(type, tokens, imports, typemap);
        }
        // if there are any declarations following the enum values, the values must be terminated by a semicolon
        if(tokens.current.value !== '}') {
            semicolon(tokens);
        }
    }
    if (!tokens.isValue('}')) {
        typeBody(type, tokens, owner, imports, typemap);
        tokens.expectValue('}');
    }
}

/**
* @param {SourceType} type 
* @param {TokenList} tokens 
* @param {{types:SourceType[]}} owner
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function typeBody(type, tokens, owner, imports, typemap) {
    for (;;) {
        let modifiers = [], annotations = [];
        while (tokens.current.kind === 'modifier') {
            modifiers.push(tokens.current);
            tokens.inc();
        }
        switch(tokens.current.kind) {
            case 'ident':
            case 'primitive-type':
                fmc(tokens.getLastMLC(), modifiers, annotations, [], type, tokens, imports, typemap);
                continue;
            case 'type-kw':
                sourceType(tokens.getLastMLC(), modifiers, tokens, type, tokens.current.value, owner, imports, typemap);
                continue;
        }
        switch(tokens.current.value) {
            case '<':
                const docs = tokens.getLastMLC();
                const type_variables = typeVariableList(type, tokens, type, imports, typemap);
                fmc(docs, modifiers, annotations, type_variables, type, tokens, imports, typemap);
                continue;
            case '@':
                tokens.inc().value === 'interface' 
                    ? sourceType(tokens.getLastMLC(), modifiers, tokens, type, '@interface', owner, imports, typemap)
                    : annotation(tokens, type, imports, typemap);
                continue;
            case ';':
                tokens.inc();
                tokens.clearMLC();
                continue;
            case '{':
                initer(tokens.getLastMLC(), tokens, type, modifiers.splice(0,1e9));
                continue;
            case '}':
                tokens.clearMLC();
                return;
        }
        if (!tokens.inc()) {
            break;
        }
    }
}

/**
 * @param {string} docs 
 * @param {Token[]} modifiers 
 * @param {SourceAnnotation[]} annotations 
 * @param {TypeVariable[]} type_vars
 * @param {SourceType} type 
 * @param {TokenList} tokens 
 * @param {ResolvedImport[]} imports 
 * @param {Map<string,CEIType>} typemap 
 */
function fmc(docs, modifiers, annotations, type_vars, type, tokens, imports, typemap) {
    let decl_type_ident = typeIdent(tokens, type, imports, typemap, { no_array_qualifiers: false, type_vars });
    if (decl_type_ident.resolved.rawTypeSignature === type.rawTypeSignature) {
        if (tokens.current.value === '(') {
            // constructor
            const { parameters, throws, body } = methodDeclaration(type_vars, type, tokens, imports, typemap);
            const ctr = new SourceConstructor(type, docs, type_vars, modifiers, parameters, throws, body);
            type.constructors.push(ctr);
            return;
        }
    }
    let name = tokens.current;
    if (!tokens.isKind('ident')) {
        name = null;
        addproblem(tokens, ParseProblem.Error(tokens.current, `Identifier expected`))
    }
    if (tokens.current.value === '(') {
        const { postnamearrdims, parameters, throws, body } = methodDeclaration(type_vars, type, tokens, imports, typemap);
        if (postnamearrdims > 0) {
            decl_type_ident.resolved = new ArrayType(decl_type_ident.resolved, postnamearrdims);
        }
        const method = new SourceMethod(type, docs, type_vars, modifiers, annotations, decl_type_ident, name, parameters, throws, body);
        type.methods.push(method);
    } else {
        if (name) {
            if (type_vars.length) {
                addproblem(tokens, ParseProblem.Error(tokens.current, `Fields cannot declare type variables`));
            }
            const locals = var_ident_list(modifiers, decl_type_ident, name, tokens, new MethodDeclarations(), type, imports, typemap);
            const fields = locals.map(l => new SourceField(type, docs, modifiers, l.typeIdent, l.decltoken, l.init));
            type.fields.push(...fields);
        }
        semicolon(tokens);
    }
}

/**
 * @param {string} docs
 * @param {TokenList} tokens 
 * @param {SourceType} type 
 * @param {Token[]} modifiers 
 */
function initer(docs, tokens, type, modifiers) {
    const i = new SourceInitialiser(type, docs, modifiers, skipBody(tokens));
    type.initers.push(i);
}

/**
 * 
 * @param {TokenList} tokens 
 */
function skipBody(tokens) {
    let body = null;
    const start_idx = tokens.idx;
    if (tokens.expectValue('{')) {
        // skip the method body
        for (let balance=1; balance;) {
            switch (tokens.current.value) {
                case '{': balance++; break;
                case '}': {
                    if (--balance === 0) {
                        body = tokens.tokens.slice(start_idx, tokens.idx + 1);
                    }
                    break;
                }
            }
            tokens.inc();
        }
    }
    return body;
}

/**
 * 
 * @param {TokenList} tokens 
 */
function annotation(tokens, scope, imports, typemap) {
    if (tokens.current.kind !== 'ident') {
        addproblem(tokens, ParseProblem.Error(tokens.current, `Type identifier expected`));
        return;
    }
    let annotation_type = typeIdent(tokens, scope, imports, typemap, {no_array_qualifiers: true, type_vars:[]});
    if (tokens.isValue('(')) {
        if (!tokens.isValue(')')) {
            expressionList(tokens, new MethodDeclarations(), scope, imports, typemap);
            tokens.expectValue(')');
        }
    }
    return new SourceAnnotation(annotation_type);
}
    
/**
 * @param {string} package_name
 * @param {Scope} scope
 * @param {string} docs
 * @param {Token[]} modifiers
 * @param {string} typeKind
 * @param {Token} kind_token
 * @param {TokenList} tokens 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap
 */
function typeDeclaration(package_name, scope, docs, modifiers, typeKind, kind_token, tokens, imports, typemap) {
    let name = tokens.inc();
    if (!tokens.isKind('ident')) {
        name = null;
        addproblem(tokens, ParseProblem.Error(tokens.current, `Type identifier expected`));
        return;
    }
    const type_short_sig = NamedSourceType.getShortSignature(package_name, scope, name.value);
    // the source type object should already exist in the type map
    /** @type {NamedSourceType} */
    // @ts-ignore
    let type = typemap.get(type_short_sig);
    if (type instanceof NamedSourceType) {
        // update the missing parts
        type.setModifierTokens(modifiers);
        type.docs = docs;
    } else {
        type = new NamedSourceType(package_name, scope, docs, modifiers, typeKind, kind_token, name, typemap);
    }
    type.typeVariables = tokens.current.value === '<'
        ? typeVariableList(type, tokens, scope, imports, typemap)
        : [];

    return type;
}

/**
 * @param {CEIType} owner
 * @param {TokenList} tokens 
 * @param {Scope} scope
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap
 */
function typeVariableList(owner, tokens, scope, imports, typemap) {
    tokens.expectValue('<');
    /** @type {TypeVariable[]} */
    const type_variables = [];
    for (;;) {
        let name = tokens.current, bounds = [];
        if (!tokens.isKind('ident')) {
            name = null;
            addproblem(tokens, ParseProblem.Error(tokens.current, `Type identifier expected`));
        }
        switch (tokens.current.value) {
            case 'extends':
            case 'super':
                tokens.inc();
                const {resolved: type_bounds} = typeIdent(tokens, scope, imports, typemap);
                bounds.push(new TypeVariable.Bound(owner, type_bounds.typeSignature, type_bounds.typeKind === 'interface'));
                break;
        }
        if (name) {
            type_variables.push(new TypeVariable(owner, name.value, bounds));
            if (tokens.isValue(',')) {
                continue;
            }
        }
        if (tokens.current.kind === 'ident') {
            addproblem(tokens, ParseProblem.Error(tokens.current, `Missing comma`));
            continue;
        }
        tokens.expectValue('>');
        break;
    }
    return type_variables;
}


/**
 * @param {TypeVariable[]} type_vars
 * @param {SourceType} owner 
 * @param {TokenList} tokens 
 * @param {ResolvedImport[]} imports 
 * @param {Map<string,CEIType>} typemap 
 */
function methodDeclaration(type_vars, owner, tokens, imports, typemap) {
    tokens.expectValue('(');
    let parameters = [], throws = [], postnamearrdims = 0, body = null;
    if (!tokens.isValue(')')) {
        for(;;) {
            const p = parameterDeclaration(type_vars, owner, tokens, imports, typemap);
            parameters.push(p);
            if (tokens.isValue(',')) {
                continue;
            }
            tokens.expectValue(')');
            break;
        }
    }
    while (tokens.isValue('[')) {
        postnamearrdims += 1;
        tokens.expectValue(']');
    }
    if (tokens.isValue('throws')) {
        throws = typeIdentList(tokens, owner, imports, typemap);
    }
    if (!tokens.isValue(';')) {
        body = skipBody(tokens);
    }
    return {
        postnamearrdims,
        parameters,
        throws,
        body,
    }
}

/**
 * @param {TypeVariable[]} type_vars 
 * @param {SourceType} owner 
 * @param {TokenList} tokens 
 * @param {ResolvedImport[]} imports 
 * @param {Map<string,CEIType>} typemap 
 */
function parameterDeclaration(type_vars, owner, tokens, imports, typemap) {
    const modifiers = [];
    while (tokens.current.kind === 'modifier') {
        modifiers.push(tokens.current);
        tokens.inc();
    }
    let type_ident = typeIdent(tokens, owner, imports, typemap, { no_array_qualifiers: false, type_vars });
    const varargs = tokens.isValue('...');
    let name_token = tokens.current;
    if (!tokens.isKind('ident')) {
        name_token = null;
        addproblem(tokens, ParseProblem.Error(tokens.current, `Identifier expected`))
    }
    let postnamearrdims = 0;
    while (tokens.isValue('[')) {
        postnamearrdims += 1;
        tokens.expectValue(']');
    }
    if (postnamearrdims > 0) {
        type_ident.resolved = new ArrayType(type_ident.resolved, postnamearrdims);
    }
    if (varargs) {
        type_ident.resolved = new ArrayType(type_ident.resolved, 1);
    }
    return new SourceParameter(modifiers, type_ident, varargs, name_token);
}

/**
* @param {SourceType} type 
* @param {TokenList} tokens 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function enumValueList(type, tokens, imports, typemap) {
    for (;;) {
        const docs = tokens.getLastMLC();
        const ident = tokens.getIfKind('ident');
        if (!ident) {
            addproblem(tokens, ParseProblem.Error(tokens.current, `Identifier expected`));
        }
        let ctr_args = [];
        if (tokens.isValue('(')) {
            if (!tokens.isValue(')')) {
                ({ expressions: ctr_args } = expressionList(tokens, new MethodDeclarations(), type, imports, typemap));
                tokens.expectValue(')');
            }
        }
        let anonymousEnumType = null;
        if (tokens.isValue('{')) {
            // anonymous enum type - just skip for now
            for (let balance = 1;;) {
                if (tokens.isValue('{')) {
                    balance++;
                } else if (tokens.isValue('}')) {
                    if (--balance === 0) {
                        break;
                    }
                } else tokens.inc();
            }
        }
        type.addEnumValue(docs, ident, ctr_args, anonymousEnumType);
        if (tokens.isValue(',')) {
            continue;
        }
        if (tokens.current.kind === 'ident') {
            addproblem(tokens, ParseProblem.Error(tokens.current, `Missing comma`));
            continue;
        }
        break;
    }
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {SourceMethodLike} method 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function statementBlock(tokens, mdecls, method, imports, typemap) {
    const block = new Block(method, tokens.current);
    tokens.expectValue('{');
    mdecls.pushScope();
    while (!tokens.isValue('}')) {
        const s = statement(tokens, mdecls, method, imports, typemap);
        block.statements.push(s);
    }
    block.decls = mdecls.popScope();
    return block;
}

/**
 * @param {TokenList} tokens
 */
function semicolon(tokens) {
    if (tokens.isValue(';')) {
        return;
    }
    addproblem(tokens, ParseProblem.Error(tokens.previous, 'Missing operator or semicolon'));
}

/**
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function statementKeyword(tokens, mdecls, method, imports, typemap) {
    let s;
    switch (tokens.current.value) {
        case 'if':
            s = new IfStatement(method, tokens.consume());
            s.test = bracketedTest(tokens, mdecls, method, imports, typemap);
            s.statement = statement(tokens, mdecls, method, imports, typemap);
            if (tokens.isValue('else')) {
                s.elseStatement = statement(tokens, mdecls, method, imports, typemap);
            }
            break;
        case 'while':
            s = new WhileStatement(method, tokens.consume());
            s.test = bracketedTest(tokens, mdecls, method, imports, typemap);
            s.statement = statement(tokens, mdecls, method, imports, typemap);
            break;
        case 'break':
            s = new BreakStatement(method, tokens.consume());
            s.target = tokens.getIfKind('ident');
            semicolon(tokens);
            break;
        case 'continue':
            s = new ContinueStatement(method, tokens.consume());
            s.target = tokens.getIfKind('ident');
            semicolon(tokens);
            break;
        case 'switch':
            s = new SwitchStatement(method, tokens.consume());
            switchBlock(s, tokens, mdecls, method, imports, typemap);
            break;
        case 'do':
            s = new DoStatement(method, tokens.consume());
            s.block = statementBlock(tokens, mdecls, method, imports, typemap);
            tokens.expectValue('while');
            s.test = bracketedTest(tokens, mdecls, method, imports, typemap);
            semicolon(tokens);
            break;
        case 'try':
            s = new TryStatement(method, tokens.consume());
            tryStatement(s, tokens, mdecls, method, imports, typemap);
            break;
        case 'return':
            s = new ReturnStatement(method, tokens.consume());
            s.expression = isExpressionStart(tokens.current) ? expression(tokens, mdecls, method, imports, typemap) : null;
            semicolon(tokens);
            break;
        case 'throw':
            s = new ThrowStatement(method, tokens.consume());
            if (!tokens.isValue(';')) {
                s.expression = isExpressionStart(tokens.current) ? expression(tokens, mdecls, method, imports, typemap) : null;
                semicolon(tokens);
            }
            break;
        case 'for':
            s = new ForStatement(method, tokens.consume());
            mdecls.pushScope();
            forStatement(s, tokens, mdecls, method, imports, typemap);
            mdecls.popScope();
            break;
        case 'synchronized':
            s = new SynchronizedStatement(method, tokens.consume());
            synchronizedStatement(s, tokens, mdecls, method, imports, typemap);
            break;
        case 'assert':
            s = new AssertStatement(method, tokens.consume());
            assertStatement(s, tokens, mdecls, method, imports, typemap);
            semicolon(tokens);
            break;
        default:
            s = new InvalidStatement(method, tokens.consume());
            break;
    }
    return s;
}

/**
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {Scope} scope 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function bracketedTest(tokens, mdecls, scope, imports, typemap) {
    tokens.expectValue('(');
    const e = expression(tokens, mdecls, scope, imports, typemap);
    tokens.expectValue(')');
    return e;
}

/**
* @param {TryStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function tryStatement(s, tokens, mdecls, method, imports, typemap) {
    mdecls.pushScope();
    let is_try_with_resources = false;
    if (tokens.isValue('(')) {
        // try-with-resources
        is_try_with_resources = true;
        for (;;) {
            const x = expression_or_var_decl(tokens, mdecls, method, imports, typemap);
            s.resources.push(x);
            if (Array.isArray(x)) {
                mdecls.locals.push(...x);
            }
            if (tokens.isValue(';')) {
                if (tokens.current.value !== ')') {
                    continue;
                }
            }
            break;
        }
        tokens.expectValue(')')
    }
    s.block = statementBlock(tokens, mdecls, method, imports, typemap);
    if (/^(catch|finally)$/.test(tokens.current.value)) {
        catchFinallyBlocks(s, tokens, mdecls, method, imports, typemap);
    }
    mdecls.popScope();
}

/**
* @param {ForStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function forStatement(s, tokens, mdecls, method, imports, typemap) {
    tokens.expectValue('(');
    if (!tokens.isValue(';')) {
        s.init = expression_list_or_var_decl(tokens, mdecls, method, imports, typemap);
        // s.init is always an array, so we need to check the element type
        if (s.init[0] instanceof Local) {
            // @ts-ignore
            addLocals(tokens, mdecls, s.init);
        }
        if (tokens.current.value === ':') {
            enhancedFor(s, tokens, mdecls, method, imports, typemap);
            return;
        }
        semicolon(tokens);
    }
    // for-condition
    if (!tokens.isValue(';')) {
        s.test = expression(tokens, mdecls, method, imports, typemap);
        semicolon(tokens);
    }
    // for-updated
    if (!tokens.isValue(')')) {
        ({ expressions: s.update } = expressionList(tokens, mdecls, method, imports, typemap));
        tokens.expectValue(')');
    }
    s.statement = statement(tokens, mdecls, method, imports, typemap);
}

/**
* @param {ForStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function enhancedFor(s, tokens, mdecls, method, imports, typemap) {
    const colon = tokens.current;
    tokens.inc();
    // enhanced for
    s.iterable = expression(tokens, mdecls, method, imports, typemap);
    tokens.expectValue(')');
    s.statement = statement(tokens, mdecls, method, imports, typemap);
}

/**
* @param {SynchronizedStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function synchronizedStatement(s, tokens, mdecls, method, imports, typemap) {
    tokens.expectValue('(');
    s.expression = expression(tokens, mdecls, method, imports, typemap);
    tokens.expectValue(')');
    s.statement = statement(tokens, mdecls, method, imports, typemap);
}

/**
* @param {AssertStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function assertStatement(s, tokens, mdecls, method, imports, typemap) {
    s.expression = expression(tokens, mdecls, method, imports, typemap);
    if (tokens.isValue(':')) {
        s.message = expression(tokens, mdecls, method, imports, typemap);
    }
}

/**
* @param {TryStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function catchFinallyBlocks(s, tokens, mdecls, method, imports, typemap) {
    for (;;) {
        if (tokens.isValue('finally')) {
            s.catches.push(statementBlock(tokens, mdecls, method, imports, typemap));
            continue;
        }
        if (tokens.isValue('catch')) {
            const catchinfo = {
                types: [],
                name: null,
                block: null,
            }
            tokens.expectValue('(');
            const mods = [];
            while (tokens.current.kind === 'modifier') {
                mods.push(tokens.current);
                tokens.inc();
            }
            let t = catchType(tokens, mdecls, method, imports, typemap);
            if (t) catchinfo.types.push(t);
            while (tokens.isValue('|')) {
                let t = catchType(tokens, mdecls, method, imports, typemap);
                if (t) catchinfo.types.push(t);
            }
            if (tokens.current.kind === 'ident') {
                catchinfo.name = tokens.current;
                tokens.inc();
            } else {
                addproblem(tokens, ParseProblem.Error(tokens.current, `Variable identifier expected`));
            }
            tokens.expectValue(')');
            mdecls.pushScope();
            let exceptionVar;
            if (catchinfo.types[0] && catchinfo.name) {
                exceptionVar = new Local(mods, catchinfo.name.value, catchinfo.name, catchinfo.types[0], 0, null);
                mdecls.locals.push(exceptionVar);
            }
            catchinfo.block = statementBlock(tokens, mdecls, method, imports, typemap);
            s.catches.push(catchinfo);
            mdecls.popScope();
            continue;
        }
        return;
    }
}

/**
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function catchType(tokens, mdecls, method, imports, typemap) {
    const t = qualifiedTerm(tokens, mdecls, method, imports, typemap);
    if (t.types[0]) {
        return t.types[0];
    }
    addproblem(tokens, ParseProblem.Error(tokens.current, `Missing or invalid type`));
    return new UnresolvedType(t.source);
}
    
/**
* @param {SwitchStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function switchBlock(s, tokens, mdecls, method, imports, typemap) {
    tokens.expectValue('(');
    s.test = expression(tokens, mdecls, method, imports, typemap);
    tokens.expectValue(')');
    tokens.expectValue('{');
    while (!tokens.isValue('}')) {
        if (/^(case|default)$/.test(tokens.current.value)) {
            caseBlock(s, tokens, mdecls, method, imports, typemap);
            continue;
        }
        addproblem(tokens, ParseProblem.Error(tokens.current, 'case statement expected'));
        break;
    }
    return s;
}

/**
* @param {SwitchStatement} s
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function caseBlock(s, tokens, mdecls, method, imports, typemap) {
    const case_start_idx = s.cases.length;
    caseExpressionList(s.cases, tokens, mdecls, method, imports, typemap);
    const statements = [];
    for (;;) {
        if (/^(case|default|\})$/.test(tokens.current.value)) {
            break;
        }
        const s = statement(tokens, mdecls, method, imports, typemap);
        statements.push(s);
    }
    s.caseBlocks.push({
        cases: s.cases.slice(case_start_idx),
        statements,
    });
}

/**
* @param {(ResolvedIdent|boolean)[]} cases
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function caseExpressionList(cases, tokens, mdecls, method, imports, typemap) {
    let c = caseExpression(tokens, mdecls, method, imports, typemap);
    if (!c) {
        return;
    }
    while (c) {
        cases.push(c);
        c = caseExpression(tokens, mdecls, method, imports, typemap);
    }
}

/**
* @param {TokenList} tokens 
* @param {MethodDeclarations} mdecls
* @param {SourceMethodLike} method 
* @param {ResolvedImport[]} imports
* @param {Map<string,CEIType>} typemap 
*/
function caseExpression(tokens, mdecls, method, imports, typemap) {
    /** @type {boolean|ResolvedIdent} */
    let e = tokens.isValue('default');
    if (!e) {
        if (tokens.isValue('case')) {
            e = expression(tokens, mdecls, method, imports, typemap);
        }
    }
    if (e) {
        tokens.expectValue(':');
    }
    return e;
}

/**
 * 
 * @param {Token[]} mods 
 * @param {SourceTypeIdent} type 
 * @param {Token} first_ident 
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls 
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports 
 * @param {Map<string,CEIType>} typemap 
 */
function var_ident_list(mods, type, first_ident, tokens, mdecls, scope, imports, typemap) {
    const new_locals = [];
    for (;;) {
        let name;
        if (first_ident && !new_locals[0]) {
            name = first_ident;
        } else {
            name = tokens.current;
            if (!tokens.isKind('ident')) {
                name = null;
                addproblem(tokens, ParseProblem.Error(tokens.current, `Variable name expected`));
            }
        }
        // look for [] after the variable name
        let postnamearrdims = 0;
        while (tokens.isValue('[')) {
            postnamearrdims += 1;
            tokens.expectValue(']');
        }
        let init = null, op = tokens.current;
        if (tokens.isValue('=')) {
            init = expression(tokens, mdecls, scope, imports, typemap);
        }
        // only add the local if we have a name
        if (name) {
            const local = new Local(mods, name.value, name, type, postnamearrdims, init);
            new_locals.push(local);
        }
        if (tokens.isValue(',')) {
            continue;
        }
        break;
    }
    return new_locals;
}
    
/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 * @returns {ResolvedIdent|Local[]}
 */
function expression_or_var_decl(tokens, mdecls, scope, imports, typemap) {

    /** @type {ResolvedIdent} */
    let matches = expression(tokens, mdecls, scope, imports, typemap);

    // if theres at least one type followed by an ident, we assume a variable declaration
    if (matches.types[0] && tokens.current.kind === 'ident') {
        return var_ident_list([], new SourceTypeIdent(matches.tokens, matches.types[0]), null, tokens, mdecls, scope, imports, typemap);
    }

    return matches;
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 * @returns {ResolvedIdent[]|Local[]}
 */
function expression_list_or_var_decl(tokens, mdecls, scope, imports, typemap) {
    let e = expression_or_var_decl(tokens, mdecls, scope, imports, typemap);
    if (Array.isArray(e)) {
        // local var decl
        return e;
    }
    const expressions = [e];
    while (tokens.isValue(',')) {
        e = expression(tokens, mdecls, scope, imports, typemap);
        expressions.push(e);
    }
    return expressions;
}

/**
 * Operator precedence levels.
 * Lower number = higher precedence.
 * Operators with equal precedence are evaluated left-to-right.
 */
const operator_precedences = {
    '*': 1, '%': 1, '/': 1,
    '+': 2, '-': 2,
    '<<': 3, '>>': 3, '>>>': 3,
    '<': 4, '>': 4, '<=': 4, '>=': 4, 'instanceof': 4,
    '==': 5, '!=': 5,
    '&': 6, '^': 7, '|': 8,
    '&&': 9, '||': 10,
    '?': 11,
    '=': 12,
    '+=':12,'-=':12,'*=':12,'/=':12,'%=':12,
    '<<=':12,'>>=':12, '&=':12, '|=':12, '^=':12,
    '&&=':12, '||=':12,
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function expression(tokens, mdecls, scope, imports, typemap, precedence_stack = [13]) {
    tokens.mark();
    /** @type {ResolvedIdent} */
    let matches = qualifiedTerm(tokens, mdecls, scope, imports, typemap);

    for(;;) {
        if (!/^(assignment|equality|comparison|bitwise|shift|logical|muldiv|plumin|instanceof)-operator/.test(tokens.current.kind) && !/\?/.test(tokens.current.value)) {
            break;
        }
        const binary_operator = tokens.current;
        const operator_precedence = operator_precedences[binary_operator.source];
        if (operator_precedence > precedence_stack[0]) {
            // bigger number -> lower precendence -> end of (sub)expression
            break;
        }
        if (operator_precedence === precedence_stack[0] && binary_operator.source !== '?' && binary_operator.kind !== 'assignment-operator') {
            // equal precedence, ltr evaluation
            break;
        }
        tokens.inc();
        // higher or equal precendence with rtl evaluation
        const rhs = expression(tokens, mdecls, scope, imports, typemap, [operator_precedence, ...precedence_stack]);

        if (binary_operator.value === '?') {
            tokens.expectValue(':');
            const falseStatement = expression(tokens, mdecls, scope, imports, typemap, [operator_precedence, ...precedence_stack]);
            matches = new ResolvedIdent(`${matches.source} ? ${rhs.source} : ${falseStatement.source}`, [new TernaryOpExpression(matches, rhs, falseStatement)]);
        } else {
            matches = new ResolvedIdent(`${matches.source} ${binary_operator.value} ${rhs.source}`, [new BinaryOpExpression(matches, binary_operator, rhs)]);
        }
    }

    matches.tokens = tokens.markEnd();
    return matches;
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function qualifiedTerm(tokens, mdecls, scope, imports, typemap) {
    let matches = rootTerm(tokens, mdecls, scope, imports, typemap);
    const postfix_operator = tokens.getIfKind('inc-operator');
    if (postfix_operator) {
        return new ResolvedIdent(`${matches.source}${postfix_operator.value}`, [new IncDecExpression(matches, postfix_operator, 'postfix')]);
    }
    matches = qualifiers(matches, tokens, mdecls, scope, imports, typemap);
    return matches;
}

/**
 * 
 * @param {Token} token 
 */
function isExpressionStart(token) {
    return /^(ident|primitive-type|[\w-]+-literal|(inc|plumin|unary)-operator|open-bracket|new-operator)$/.test(token.kind);
}

/**
 * @param {Token} token first token following the close bracket
 * @param {ResolvedIdent} matches - the bracketed expression
 */
function isCastExpression(token, matches) {
    // working out if this is supposed to be a cast expression is problematic.
    //   (a) + b     -> cast or binary expression (depends on how a is resolved)
    // if the bracketed expression cannot be resolved:
    //   (a) b     -> assumed to be a cast
    //   (a) + b   -> assumed to be an expression
    //   (a) 5   -> assumed to be a cast
    //   (a) + 5   -> assumed to be an expression
    if (matches.types[0] && !(matches.types[0] instanceof AnyType)) {
        // resolved type - this must be a cast
        return true;
    }
    if (!matches.types[0]) {
        // not a type - this must be an expression
        return false;
    }
    // if we reach here, the type is AnyType - we assume a cast if the next
    // value is the start of an expression, except for +/-
    if (token.kind === 'plumin-operator') {
        return false;
    }
    return isExpressionStart(token);
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 * @returns {ResolvedIdent}
 */
function rootTerm(tokens, mdecls, scope, imports, typemap) {
    /** @type {ResolvedIdent} */
    let matches;
    switch(tokens.current.kind) {
        case 'ident':
            matches = resolveIdentifier(tokens, mdecls, scope, imports, typemap);
            break;
        case 'primitive-type':
            matches = new ResolvedIdent(tokens.current, [], [], [PrimitiveType.fromName(tokens.current.value)]);
            break;
        case 'string-literal':
            matches = new ResolvedIdent(tokens.current, [new StringLiteral(tokens.current, typemap.get('java/lang/String'))]);
            break;
        case 'char-literal':
            matches = new ResolvedIdent(tokens.current, [new CharacterLiteral(tokens.current)]);
            break;
        case 'boolean-literal':
            matches = new ResolvedIdent(tokens.current, [new BooleanLiteral(tokens.current)]);
            break;
        case 'object-literal':
            // this, super or null
            const scoped_type = scope instanceof SourceType ? scope : scope.owner;
            if (tokens.current.value === 'this' || tokens.current.value === 'super') {
                matches = new ResolvedIdent(tokens.current, [new InstanceLiteral(tokens.current, scoped_type)]);
            } else {
                matches = new ResolvedIdent(tokens.current, [new NullLiteral(tokens.current)]);
            }
            break;
        case /number-literal/.test(tokens.current.kind) && tokens.current.kind:
            matches = new ResolvedIdent(tokens.current, [NumberLiteral.from(tokens.current)]);
            break;
        case 'inc-operator':
            let incop = tokens.consume();
            matches = qualifiedTerm(tokens, mdecls, scope, imports, typemap);
            return new ResolvedIdent(`${incop.value}${matches.source}`, [new IncDecExpression(matches, incop, 'prefix')])
        case 'plumin-operator':
        case 'unary-operator':
            let unaryop = tokens.consume();
            matches = qualifiedTerm(tokens, mdecls, scope, imports, typemap);
            let unary_value = matches.variables[0] instanceof NumberLiteral
                ? NumberLiteral[unaryop.value](matches.variables[0])
                : new UnaryOpExpression(matches, unaryop);
            return new ResolvedIdent(`${unaryop.value}${matches.source}`, [unary_value])
        case 'new-operator':
            return newTerm(tokens, mdecls, scope, imports, typemap);
        case 'open-bracket':
            tokens.inc();
            if (tokens.isValue(')')) {
                // parameterless lambda
                tokens.expectValue('->');
                let ident, lambdaBody = null;
                if (tokens.current.value === '{') {
                    // todo - parse lambda body
                    skipBody(tokens);
                } else {
                    lambdaBody = expression(tokens, mdecls, scope, imports, typemap);
                    ident = `() -> ${lambdaBody.source}`;
                }
                return new ResolvedIdent(ident, [new LambdaExpression([], lambdaBody)]);
            }
            matches = expression(tokens, mdecls, scope, imports, typemap);
            tokens.expectValue(')');
            if (isCastExpression(tokens.current, matches)) {
                // typecast
                const expression = qualifiedTerm(tokens, mdecls, scope, imports, typemap)
                return new ResolvedIdent(`(${matches.source})${expression.source}`, [new CastExpression(matches, expression)]);
            }
            // the result of a bracketed expression is always a value, never a variable
            // - this prevents things like: (a) = 5;
            return new ResolvedIdent(`(${matches.source})`, [new BracketedExpression(matches)]);
        case tokens.current.value === '{' && 'symbol':
            // array initer
            let elements = [], open = tokens.current;
            tokens.expectValue('{');
            if (!tokens.isValue('}')) {
                ({ expressions: elements } = expressionList(tokens, mdecls, scope, imports, typemap, { isArrayLiteral:true }));
                tokens.expectValue('}');
            }
            const ident = `{${elements.map(e => e.source).join(',')}}`;
            return new ResolvedIdent(ident, [new ArrayValueExpression(elements, open)]);
        default:
            addproblem(tokens, ParseProblem.Error(tokens.current, 'Expression expected'));
            return new ResolvedIdent('', [new AnyValue('')]);
    }
    tokens.inc();
    return matches;
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function newTerm(tokens, mdecls, scope, imports, typemap) {
    tokens.mark();
    const new_token = tokens.current;
    tokens.expectValue('new');
    const ctr_type = typeIdent(tokens, scope, imports, typemap, {no_array_qualifiers:true, type_vars:[]});
    let match = new ResolvedIdent(`new ${ctr_type.resolved.simpleTypeName}`, [], [], []);
    let newtokens;
    switch(tokens.current.value) {
        case '[':
            match = arrayQualifiers(match, tokens, mdecls, scope, imports, typemap);
            newtokens = tokens.markEnd();
            // @ts-ignore
            if (tokens.current.value === '{') {
                // array init
                rootTerm(tokens, mdecls, scope, imports, typemap);
            }
            return new ResolvedIdent(match.source, [new NewArray(new_token, ctr_type, match)], [], [], '', newtokens);
        case '(':
            let ctr_args = [], commas = [], type_body = null;
            let open_bracket = tokens.consume();
            if (!tokens.isValue(')')) {
                ({ expressions: ctr_args, commas } = expressionList(tokens, mdecls, scope, imports, typemap));
                tokens.expectValue(')');
            }
            newtokens = tokens.markEnd();
            // @ts-ignore
            if (tokens.current.value === '{') {
                // anonymous type
                tokens.consume();
                type_body = new AnonymousSourceType(ctr_type, scope, typemap);
                typemap.set(type_body.shortSignature, type_body);
                typeBody(type_body, tokens, mdecls, imports, typemap);
                tokens.expectValue('}');
                // perform an immediate parse of all the methods in the anonymous class
                parseTypeMethods(type_body, imports, typemap);
            }
            return new ResolvedIdent(match.source, [new NewObject(new_token, ctr_type, open_bracket, ctr_args, commas, type_body)], [], [], '', newtokens);
    }
    newtokens = tokens.markEnd();
    addproblem(tokens, ParseProblem.Error(tokens.current, 'Constructor expression expected'));
    return match;
}

/**
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 * @param {{isArrayLiteral: boolean}} [opts]
 */
function expressionList(tokens, mdecls, scope, imports, typemap, opts) {
    let e = expression(tokens, mdecls, scope, imports, typemap);
    const expressions = [e];
    const commas = [];
    while (tokens.current.value === ',') {
        commas.push(tokens.consume());
        if (opts && opts.isArrayLiteral) {
            // array literals are allowed a single trailing comma
            // @ts-ignore
            if (tokens.current.value === '}') {
                break;
            }
        }
        e = expression(tokens, mdecls, scope, imports, typemap);
        expressions.push(e);
    }
    return { expressions, commas };
}

/**
 * @param {ResolvedIdent} instance 
 * @param {ResolvedIdent} index
 */
function arrayElementOrConstructor(instance, index) {
    const ident = `${instance.source}[${index.source}]`;
    const types = instance.types.map(t => new FixedLengthArrayType(t, index));
    return new ResolvedIdent(ident, [new ArrayIndexExpression(instance, index)], [], types, '', index.tokens.slice());
}

/**
 * @param {ResolvedIdent} matches
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function qualifiers(matches, tokens, mdecls, scope, imports, typemap) {
    for (;;) {
        switch (tokens.current.value) {
            case '.':
                matches = memberQualifier(matches, tokens, mdecls, scope, imports, typemap);
                break;
            case '[':
                matches = arrayQualifiers(matches, tokens, mdecls, scope, imports, typemap);
                break;
            case '(':
                // method or constructor call
                matches = methodCallQualifier(matches, tokens, mdecls, scope, imports, typemap);
                break;
            case '<':
                // generic type arguments - since this can be confused with less-than, only parse
                // it if there is at least one type
                if (!matches.types[0]) {
                    return matches;
                }
                tokens.inc();
                genericTypeArgs(tokens, matches.types, scope, imports, typemap);
                break;
            default:
                return matches;
        }
    }
}

/**
 * @param {ResolvedIdent} matches
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function memberQualifier(matches, tokens, mdecls, scope, imports, typemap) {
    tokens.mark();
    const dot = tokens.consume();
    let expr, label = `${matches.source}.${tokens.current.value}`;
    let types = [], package_name = '';
    switch (tokens.current.value) {
        case 'class':
        case 'this':
            expr = new MemberExpression(matches, dot, tokens.consume());
            break;
        default:
            let member = tokens.getIfKind('ident');
            if (member) {
                if (matches.package_name || matches.types[0]) {
                    ({ types, package_name } = resolveNextTypeOrPackage(member.value, matches.types, matches.package_name, typemap));
                }
            }
            expr = new MemberExpression(matches, dot, member);
            break;
    }
    return new ResolvedIdent(label, [expr], [], types, package_name, [...matches.tokens, ...tokens.markEnd()]);
}

/**
 * @param {ResolvedIdent} matches
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function arrayQualifiers(matches, tokens, mdecls, scope, imports, typemap) {
    while (tokens.current.value === '[') {
        tokens.mark();
        tokens.inc();
        if (tokens.isValue(']')) {
            // array type
            matches = arrayTypeExpression(matches, tokens.markEnd());
        } else {
            // array index
            const index = expression(tokens, mdecls, scope, imports, typemap);
            tokens.expectValue(']');
            tokens.markEnd();
            matches = arrayElementOrConstructor(matches, index);
        }
    }
    return matches;
}

/**
 * @param {ResolvedIdent} matches
 * @param {TokenList} tokens 
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function methodCallQualifier(matches, tokens, mdecls, scope, imports, typemap) {
    let args = [], commas = [];
    tokens.mark();
    const open_bracket = tokens.consume();
    if (!tokens.isValue(')')) {
        ({ expressions: args, commas } = expressionList(tokens, mdecls, scope, imports, typemap));
        tokens.expectValue(')');
    }
    return new ResolvedIdent(`${matches.source}(${args.map(a => a.source).join(', ')})`, [new MethodCallExpression(matches, open_bracket, args, commas)], [], [], '', [...matches.tokens, ...tokens.markEnd()]);
}

/**
 * @param {ResolvedIdent} matches 
 * @param {Token[]} array_tokens
 */
function arrayTypeExpression(matches, array_tokens) {
    const types = matches.types.map(t => new SourceArrayType(t));
    return new ResolvedIdent(`${matches.source}[]`, [], [], types, '', [...matches.tokens, ...array_tokens]);
}

/**
 * When resolving identifiers, we need to search across everything because
 * identifiers are context-sensitive.
 * For example, the following compiles even though C takes on different definitions within method:
 * 
 * class A {
 *   class C {
 *   }
 * }
 * 
 * class B extends A {
 *    String C;
 *    int C() {
 *       return C.length();
 *    }
 *    void method() {
 *        C obj = new C();
 *        int x = C.class.getName().length() + C.length() + C();
 *    }
 * }
 * 
 * But... parameters and locals override fields and methods (and local types override enclosed types)
 * 
 * @param {TokenList} tokens
 * @param {MethodDeclarations} mdecls
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports
 * @param {Map<string,CEIType>} typemap 
 */
function resolveIdentifier(tokens, mdecls, scope, imports, typemap) {
    const ident = tokens.current.value;
    const matches = findIdentifier(tokens.current, mdecls, scope, imports, typemap);
    checkIdentifierFound(tokens, ident, matches);
    return matches;
}

/**
 * @param {TokenList} tokens
 * @param {ResolvedIdent} matches 
 */
function checkIdentifierFound(tokens, ident, matches) {
    if (!matches.variables[0] && !matches.methods[0] && !matches.types[0] && !matches.package_name) {
        addproblem(tokens, ParseProblem.Error(tokens.current, `Unresolved identifier: ${matches.source}`));
        // pretend it matches everything
        matches.variables = [new AnyValue(matches.source)];
        matches.methods = [new AnyMethod(ident)];
        matches.types = [new AnyType(matches.source)];
    }
}

/**
 * @param {Token} token 
 * @param {MethodDeclarations} mdecls 
 * @param {Scope} scope 
 * @param {ResolvedImport[]} imports 
 * @param {Map<string,CEIType>} typemap 
 */
function findIdentifier(token, mdecls, scope, imports, typemap) {
    const ident = token.value;
    const matches = new ResolvedIdent(ident);
    matches.tokens = [token];

    // is it a local or parameter - note that locals must be ordered innermost-scope-first
    const local = mdecls.locals.find(local => local.name === ident);
    let param = scope && !(scope instanceof SourceType) && scope.parameters.find(p => p.name === ident);
    if (local || param) {
        matches.variables = [new Variable(token, local || param)];
    } else if (scope) {
        // is it a field, method or enum value in the current type (or any of the outer types or superclasses)
        const scoped_type = scope instanceof SourceType ? scope : scope.owner;
        const outer_types = [];
        for (let m, t = scoped_type._rawShortSignature;; ) {
            m = t.match(/(.+)[$][^$]+$/);
            if (!m) break;
            const enctype = typemap.get(t = m[1]);
            enctype && outer_types.push(enctype);
        }
        const inherited_types = getTypeInheritanceList(scoped_type);
        const method_sigs = new Set();
        [...inherited_types, ...outer_types].forEach(type => {
            if (!matches.variables[0]) {
                const field = type.fields.find(f => f.name === ident);
                if (field) {
                    matches.variables = [new Variable(token, field)];
                    return;
                }
                const enumValue = (type instanceof SourceType) && type.enumValues.find(e => e.ident.value === ident);
                if (enumValue) {
                    matches.variables = [new Variable(token, enumValue)];
                    return;
                }
            }
            matches.methods = matches.methods.concat(
                type.methods.filter(m => {
                    if (m.name !== ident || method_sigs.has(m.methodSignature)) {
                        return;
                    }
                    method_sigs.add(m.methodSignature);
                    return true;
                })
            );
        });
    }

    // check static imports
    imports.forEach(imp => {
        imp.members.forEach(member => {
            if (member.name === ident) {
                if (member instanceof Field) {
                    matches.variables.push(new Variable(token, member));
                } else if (member instanceof Method) {
                    matches.methods.push(member);
                }
            }
        })
    });

    const type = mdecls.types.find(t => t.simpleTypeName === ident);
    if (type) {
        matches.types = [type];
    } else {
        const { types, package_name } = resolveTypeOrPackage(ident, [], scope, imports, typemap);
        matches.types = types;
        matches.package_name = package_name;
    }

    return matches;
}


exports.addproblem = addproblem;
exports.parseTypeMethods = parseTypeMethods;
exports.parse = parse;
exports.flattenBlocks = flattenBlocks;
