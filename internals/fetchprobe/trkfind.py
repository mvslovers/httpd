# usage: trkfind.py IMAGE HEXPATTERN -- read-only: print abs track + record of every record whose data starts with HEXPATTERN
import sys, struct, zlib, bz2
fn, pat = sys.argv[1], bytes.fromhex(sys.argv[2])
f = open(fn, "rb"); dh = f.read(512); heads = struct.unpack("<I", dh[8:12])[0]
ch = f.read(512); e = ">" if ch[3] & 0x02 else "<"
numl1 = struct.unpack(e + "I", ch[4:8])[0]
l1 = struct.unpack(e + "%dI" % numl1, f.read(4 * numl1))
for i, l2pos in enumerate(l1):
    if l2pos in (0, 0xFFFFFFFF): continue
    f.seek(l2pos); l2 = [struct.unpack(e + "IHH", f.read(8)) for _ in range(256)]
    for j, (pos, ln, sz) in enumerate(l2):
        if pos == 0 or ln == 0: continue
        f.seek(pos); img = f.read(ln); comp = img[0]; body = img[5:]
        try:
            if comp & 0x01: body = zlib.decompress(body)
            elif comp & 0x02: body = bz2.decompress(body)
        except Exception: continue
        if pat not in body: continue
        p = 16; t = i * 256 + j
        while p + 8 <= len(body):
            cnt = body[p:p+8]
            if cnt == b"\xff" * 8: break
            cc, hh, r, kl, dl = struct.unpack(">HHBBH", cnt)
            if body[p+8+kl:p+8+kl+len(pat)] == pat:
                print(f"abs trk {t} = cyl {t//heads} head {t%heads} R={r} DL={dl}")
            p += 8 + kl + dl
