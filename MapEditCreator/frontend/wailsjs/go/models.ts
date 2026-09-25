export namespace main {
	
	export class BlockHint {
	    name: string;
	    desc: string;
	    funcs: string[];
	
	    static createFrom(source: any = {}) {
	        return new BlockHint(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.name = source["name"];
	        this.desc = source["desc"];
	        this.funcs = source["funcs"];
	    }
	}
	export class CompileResult {
	    output: string;
	    exitCode: number;
	    duration: string;
	    binPath: string;
	
	    static createFrom(source: any = {}) {
	        return new CompileResult(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.output = source["output"];
	        this.exitCode = source["exitCode"];
	        this.duration = source["duration"];
	        this.binPath = source["binPath"];
	    }
	}
	export class Diagnostic {
	    line: number;
	    severity: string;
	    message: string;
	
	    static createFrom(source: any = {}) {
	        return new Diagnostic(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.line = source["line"];
	        this.severity = source["severity"];
	        this.message = source["message"];
	    }
	}
	export class FuncMeta {
	    desc: string;
	    args: number;
	    sig: string;
	
	    static createFrom(source: any = {}) {
	        return new FuncMeta(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.desc = source["desc"];
	        this.args = source["args"];
	        this.sig = source["sig"];
	    }
	}
	export class EditorMeta {
	    blocks: Record<string, BlockHint>;
	    funcs: Record<string, FuncMeta>;
	
	    static createFrom(source: any = {}) {
	        return new EditorMeta(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.blocks = this.convertValues(source["blocks"], BlockHint, true);
	        this.funcs = this.convertValues(source["funcs"], FuncMeta, true);
	    }
	
		convertValues(a: any, classs: any, asMap: boolean = false): any {
		    if (!a) {
		        return a;
		    }
		    if (a.slice && a.map) {
		        return (a as any[]).map(elem => this.convertValues(elem, classs));
		    } else if ("object" === typeof a) {
		        if (asMap) {
		            for (const key of Object.keys(a)) {
		                a[key] = new classs(a[key]);
		            }
		            return a;
		        }
		        return new classs(a);
		    }
		    return a;
		}
	}

}

