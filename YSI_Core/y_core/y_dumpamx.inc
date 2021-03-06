/*
	
	Now also provides OnCodeInit, which comes before OnScriptInit and should
	mostly be used for @emit code generation and other assembly related stuff
	(since it is called by the JIT if that exists).
Legal:
	Version: MPL 1.1
	
	The contents of this file are subject to the Mozilla Public License Version 
	1.1 the "License"; you may not use this file except in compliance with 
	the License. You may obtain a copy of the License at 
	http://www.mozilla.org/MPL/
	
	Software distributed under the License is distributed on an "AS IS" basis,
	WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
	for the specific language governing rights and limitations under the
	License.
	
	The Original Code is the YSI framework.
	
	The Initial Developer of the Original Code is Alex "Y_Less" Cole.
	Portions created by the Initial Developer are Copyright C 2011
	the Initial Developer. All Rights Reserved.

Contributors:
	Y_Less
	koolk
	JoeBullet/Google63
	g_aSlice/Slice
	Misiur
	samphunter
	tianmeta
	maddinat0r
	spacemud
	Crayder
	Dayvison
	Ahmad45123
	Zeex
	irinel1996
	Yiin-
	Chaprnks
	Konstantinos
	Masterchen09
	Southclaws
	PatchwerkQWER
	m0k1
	paulommu
	udan111

Thanks:
	JoeBullet/Google63 - Handy arbitrary ASM jump code using SCTRL.
	ZeeX - Very productive conversations.
	koolk - IsPlayerinAreaEx code.
	TheAlpha - Danish translation.
	breadfish - German translation.
	Fireburn - Dutch translation.
	yom - French translation.
	50p - Polish translation.
	Zamaroht - Spanish translation.
	Los - Portuguese translation.
	Dracoblue, sintax, mabako, Xtreme, other coders - Producing other modes for
		me to strive to better.
	Pixels^ - Running XScripters where the idea was born.
	Matite - Pestering me to release it and using it.

Very special thanks to:
	Thiadmer - PAWN, whose limits continue to amaze me!
	Kye/Kalcor - SA:MP.
	SA:MP Team past, present and future - SA:MP.

Optional plugins:
	Gamer_Z - GPS.
	Incognito - Streamer.
	Me - sscanf2, fixes2, Whirlpool.
*/

static stock bool:DumpAMX_WriteAMXData(File:file, data[], &offset, size, compact) {
	new hdr[AMX_HDR];
	GetAmxHeader(hdr);

	new dat = 0;
	new end = hdr[AMX_HDR_HEA] - hdr[AMX_HDR_DAT];

	while (dat < end) {
		DumpAMX_WriteEncoded(file, data, offset, compact, ReadAmxMemory(dat), size);
		dat += 4;
	}

	return true;
}

static stock bool:DumpAMX_WriteAMXHeader(File:file, data[], &offset, size, compact) {
	#pragma unused size, compact

	new plain_amxhdr[AMX_HDR_CELLS];
	GetRawAmxHeader(plain_amxhdr);

	// write the header with masked flags
	plain_amxhdr[2] &= 0xFFFF0006;
	fblockwrite(file, plain_amxhdr, 56 / 4);

	// write the rest of the header without native function addresses
	new dat = plain_amxhdr[AMX_HDR_OFFSET_DAT / 4];

	if (dat & 0x03) {
		// `dat` should always be 4-byte aligned.
		return false;
	}

	new start;
	new end;

	// YSI plays with the public function table start so don't use it directly
	//start = plain_amxhdr[(AMX_HDR_OFFSET_PUBLICS / 4)] - dat;
	start = AMX_HDR_BYTES - dat;

	for (end = plain_amxhdr[AMX_HDR_OFFSET_NATIVES / 4] - dat; start < end; start += 8) {
		data[0] = ReadAmxMemory(start + 0);
		data[1] = ReadAmxMemory(start + 4);
		fblockwrite(file, data, 2);
	}

	// write the natives without their resolved address
	data[0] = 0;
	for (end = plain_amxhdr[AMX_HDR_OFFSET_LIBRARIES / 4] - dat; start < end; start += 8) {
		data[1] = ReadAmxMemory(start + 4);
		fblockwrite(file, data, 2);
	}

	// the first half of the header is guaranteed to be a multiple of 4 bytes long, i.e. an exact
	// number of cells.  The second half isn't.
	for (end = (plain_amxhdr[AMX_HDR_OFFSET_COD / 4] & ~0x07) - dat; start < end; start += 8) {
		data[0] = ReadAmxMemory(start + 0);
		data[1] = ReadAmxMemory(start + 4);
		fblockwrite(file, data, 2);
	}

	// straggling bytes
	offset = plain_amxhdr[AMX_HDR_OFFSET_COD / 4] - dat - start;

	data[0] = ReadAmxMemory(start + 0);
	data[1] = ReadAmxMemory(start + 4);

	return true;
}

#define ENC_MAX 5
static stock DumpAMX_EncodeBytes(c, buffer[ENC_MAX char]) {
	// ported from the main compiler to ensure compatibility

	new index = ENC_MAX - 1;
	do {
		buffer{index--} = (c & 0x7F); // store 7 bits
		c >>= 7;
	} while (index >= 0);

	// skip leading zeros
	while (index < ENC_MAX - 2 && buffer{index + 1} == 0 && (buffer{index + 2} & 0x40) == 0) {
		index++;
	}

	// skip leading -1s
	if (index == -1 && buffer{index + 1} == 0x0F && (buffer{index + 2} & 0x40) != 0) {
		index++;
	}

	while (index < ENC_MAX - 2 && buffer{index + 1} == 0x7F && (buffer{index + 2} & 0x40) != 0) {
		index++;
	}

	++index;
	new ret = index;
	while (index < ENC_MAX - 1)
	{
		buffer{index++} |= 0x80;
	}

	return ret;
}

static stock DumpAMX_WriteEncoded(File:file, data[], &offset, compact, c, size) {
	if (compact) {
		new buffer[ENC_MAX char];
		new index = DumpAMX_EncodeBytes(c, buffer);
		while (index < ENC_MAX) {
			data{offset++ ^ 0x03} = buffer{index++};
			if (offset == size) {
				// Write the whole cell.
				fblockwrite(file, data, size >>> 2);
				offset = 0;
			}
		}
	} else while (compact++ != 4) {
		data{offset++ ^ 0x03} = c & 0xFF;
		if (offset == size) {
			// Write the whole cell.
			fblockwrite(file, data, size >>> 2);
			offset = 0;
		}
		c >>>= 8;
	}
}

static stock bool:DumpAMX_WriteAMXCode(File:file, data[], &offset, size, compact) {
	new ctx[DisasmContext];
	DisasmInit(ctx);
	
	new base;
	{
		new hdr[AMX_HDR];
		GetAmxHeader(hdr);
		base = GetAmxBaseAddress() + hdr[AMX_HDR_COD];
	}

	while (DisasmGetNextIp(ctx) < ctx[DisasmContext_end_ip])
	{
		if (!DisasmDecodeInsn(ctx)) {
			new cip = DisasmGetNextIp(ctx);
			DumpAMX_WriteEncoded(file, data, offset, compact, ReadAmxMemory(cip), size);
			//fblockwrite(file, data, 1);
			continue;
		}

		new Opcode:opcode = DisasmGetOpcode(ctx);

		switch (opcode) {
			case OP_CASETBL: {
				new num = DisasmGetNumOperands(ctx);
				new rel_addr = DisasmGetOperand(ctx, 1) - base;
				DumpAMX_WriteEncoded(file, data, offset, compact, _:opcode, size);
				DumpAMX_WriteEncoded(file, data, offset, compact, num, size);
				DumpAMX_WriteEncoded(file, data, offset, compact, rel_addr, size);
				//fblockwrite(file, data, 3);
				for (new i = 1; i <= num; i++) {
					new val = DisasmGetOperand(ctx, i * 2);
					rel_addr = DisasmGetOperand(ctx, i * 2 + 1) - base;
					DumpAMX_WriteEncoded(file, data, offset, compact, val, size);
					DumpAMX_WriteEncoded(file, data, offset, compact, rel_addr, size);
					//fblockwrite(file, data, 2);
				}
			}
			case OP_SYSREQ_D: {
				// convert back to SYSREQ.C
				new address = DisasmGetOperand(ctx);
				new index = GetNativeIndexFromAddress(address);
				DumpAMX_WriteEncoded(file, data, offset, compact, _:OP_SYSREQ_C, size);
				DumpAMX_WriteEncoded(file, data, offset, compact, index, size);
				//fblockwrite(file, data, 2);
			}
			default: {
				DumpAMX_WriteEncoded(file, data, offset, compact, _:opcode, size);
				new n = DisasmGetNumOperands(ctx);
				for (new i = 0; i < n; i++) {
					//data[i + 1] = DisasmGetOperandReloc(ctx, i);
					DumpAMX_WriteEncoded(file, data, offset, compact, DisasmGetOperandReloc(ctx, i), size);
				}
				//fblockwrite(file, data, n + 1);
			}
		}
	}

	return true;
}
#undef ENC_MAX

stock bool:DumpAMX_Write(const filename[]) {
	new File:file = fopen(filename, io_write);
	if (file) {
		new
			data[12],
			offset,
			compact;

		{
			new hdr[AMX_HDR];
			GetAmxHeader(hdr);
			compact = hdr[AMX_HDR_FLAGS] & 2;
		}

		DumpAMX_WriteAMXHeader(file, data, offset, sizeof (data) * 4, compact);
		DumpAMX_WriteAMXCode(file, data, offset, sizeof (data) * 4, compact);
		DumpAMX_WriteAMXData(file, data, offset, sizeof (data) * 4, compact);

		compact = (offset + 3) / 4;
		if (compact) {
			fblockwrite(file, data, compact);
		}

		// write the new length at the start
		fseek(file, 0, seek_start);
		data[0] = flength(file) - (compact * 4 - offset);
		fblockwrite(file, data, 1);
		fclose(file);
		return true;
	}
	return false;
}

stock DumpAMX_Included_() {}

