/// <reference lib="Deno.window"/>

function uint8ArrayToHex(data:Uint8Array) : string {
	const hexes = [];
	for( const b of data ) hexes.push(((b >> 4) & 0x0F).toString(16) + (b & 0x0F).toString(16));
	return hexes.join('');
}

let hostname = "::";
let port = 16378;

for( const arg of Deno.args ) {
	let m : RegExpExecArray|null;
	if( (m = /^(?:\[(?<hostname>[^\]]+)\]|(?<hostname>[^\[\]:]+)):(?<port>\d+)$/.exec(arg)) != null ) {
		hostname = m.groups!['hostname'];
		port = +m.groups!['port'];
	} else if( (m = /^(\d+)$/.exec(arg)) != null ) {
		port = +m[1];
	} else {
		throw new Error(`Unrecognized argument: ${arg}`);
	}
}

console.log(`Listening on ${hostname}:${port}`);

const listener = Deno.listenDatagram({
	transport: "udp",
	hostname,
	port,
});
while( true ) {
	const [data, sourceAddr] = await listener.receive();
	let text;
	try {
		text = "text:\n  " + new TextDecoder().decode(data).replaceAll("\n","\n  ");
	} catch( e ) {
		text = "hex: " + uint8ArrayToHex(data);
	}
	console.log(`Received ${data.length} bytes from ${JSON.stringify(sourceAddr)}: ${text}`);
}
