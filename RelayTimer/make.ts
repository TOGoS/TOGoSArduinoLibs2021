import Builder, { BuildContext, BuildRule } from 'https://deno.land/x/tdbuilder@0.5.14/Builder.ts';

const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();

const buildRules : {[name:string]: BuildRule} = {
	"version.h": {
		description: "Generate version.h to reference the current Git commit",
		targetType: "file",
		prereqs: ["../.git/refs"],
		invoke: async (ctx) => {
			const cmd = new Deno.Command("git", { args: ["rev-parse", "HEAD"]});
			const { code, stdout, stderr } = await cmd.output();
			console.error(textDecoder.decode(stderr));
			if( code != 0 ) throw new Error(`git rev-parse HEAD exited with non-zero status: ${code}`);
			const commitHash = await textDecoder.decode(stdout).trim();
			await Deno.remove(ctx.targetName).catch(_e => { /* don't care */ });
			using versionH = await Deno.open(ctx.targetName, {write:true, createNew:true})
			versionH.write(textEncoder.encode(`constexpr const char *appVersion = \"x-git-object:${commitHash}#RelayTimer/RelayTimer.ino\";\n`));
		}
	},
};

if( import.meta.main ) {
	const builder = new Builder({
		rules: buildRules,
		globalPrerequisiteNames: ["make.ts"],
		defaultTargetNames: ["version.h"],
	});
	
	Deno.exit(await builder.processCommandLine(Deno.args));
}
