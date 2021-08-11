/**
 * @typedef {import('../body-types').ResolvedIdent} ResolvedIdent
 * @typedef {import('../body-types').ResolveInfo} ResolveInfo
 * @typedef {import('../tokenizer').Token} Token
 */
const { Expression } = require("./Expression");
const { JavaType, CEIType, PrimitiveType } = require('java-mti');
const { AnyType, MethodType, PackageNameType, TypeIdentType } = require('../anys');
const { getTypeInheritanceList } = require('../expression-resolver');
const { resolveNextPackage } = require('../type-resolver');
const ParseProblem = require('../parsetypes/parse-problem');

class MemberExpression extends Expression {
    /**
     * @param {ResolvedIdent} instance
     * @param {Token} dot
     * @param {Token|null} member
     */
    constructor(instance, dot, member) {
        super();
        this.instance = instance;
        this.dot = dot;
        // member will be null for incomplete expressions
        this.member = member;
    }

    /**
     * @param {ResolveInfo} ri 
     */
    resolveExpression(ri) {
        let instance = this.instance.resolveExpression(ri);

        if (instance instanceof AnyType) {
            return instance;
        }

        if (instance instanceof PackageNameType) {
            this.dot.loc = { key: `fqdi:${instance.package_name}` };
            if (!this.member) {
                return instance;
            }
            this.member.loc = this.dot.loc;
            const ident = this.member.value;
            const { sub_package_name, type } = resolveNextPackage(instance.package_name, ident, ri.typemap);
            if (!type && !sub_package_name) {
                ri.problems.push(ParseProblem.Error(this.member, `Unresolved identifier: '${ident}'`));
            }
            return type ? new TypeIdentType(type)
             : sub_package_name ? new PackageNameType(sub_package_name)
             : AnyType.Instance;
        }

        let loc_key = `fqi`;
        if (instance instanceof TypeIdentType) {
            loc_key = 'fqs';
            instance = instance.type;
        }

        if (!(instance instanceof JavaType)) {
            return AnyType.Instance;
        }

        this.dot.loc = { key: `${loc_key}:${instance.typeSignature}` };
        if (!this.member) {
            ri.problems.push(ParseProblem.Error(this.dot, `Identifier expected`));
            return instance;
        }
        
        this.member.loc = this.dot.loc;
        const ident = this.member.value;

        if (ident === 'this') {
            // if this has a type qualifier (Type.this), return the type, otherwise it's
            // and error and return AnyType
            return ((loc_key === 'fqs') && (instance instanceof CEIType)) ? instance : AnyType.Instance;
        }

        if (ident === 'class') {
            // if this has a type qualifier (Type.class), return the Class instance, otherwise it's
            // and error and return AnyType
            if (loc_key !== 'fqs') {
                return AnyType.Instance;
            }
            let class_type = instance;
            if (instance instanceof PrimitiveType) {
                class_type = ri.typemap.get(`java/lang/${{
                    B:'Byte',S:'Short',I:'Integer',J:'Long',F:'Float',D:'Double',C:'Character',Z:'Boolean',V:'Void'
                }[instance.typeSignature]}`)
            }
            const clz = ri.typemap.get('java/lang/Class').specialise([class_type]);
            if (!ri.typemap.has(clz.shortSignature)) {
                ri.typemap.set(clz.shortSignature, clz);
            }
            return clz;
        }

        const field = instance.fields.find(f => f.name === ident);
        if (field) {
            return field.type;
        }

        if (!(instance instanceof CEIType)) {
            ri.problems.push(ParseProblem.Error(this.member, `Unresolved member: '${ident}'`));
            return AnyType.Instance;
        }

        let methods = new Map();
        getTypeInheritanceList(instance).forEach(type => {
            type.methods.forEach(m => {
                let msig;
                if (m.name === ident && !methods.has(msig = m.methodSignature)) {
                    methods.set(msig, m);
                }
            })
        });
        if (methods.size > 0) {
            return new MethodType([...methods.values()]);
        }
        ri.problems.push(ParseProblem.Error(this.member, `Unresolved member: '${ident}' in type '${instance.fullyDottedRawName}'`));
        return AnyType.Instance;
    }

    tokens() {
        return this.member;
    }
}

exports.MemberExpression = MemberExpression;
