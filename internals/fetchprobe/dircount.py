# usage: dircount.py IMAGE CYL HEAD -- read-only: count keyed 256-byte directory blocks from the given track on, up to the first EOF
import sys, struct, zlib, bz2
fn, cyl, head = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
f = open(fn, "rb"); dh = f.read(512); heads = struct.unpack("<I", dh[8:12])[0]
ch = f.read(512); e = ">" if ch[3] & 0x02 else "<"
numl1 = struct.unpack(e + "I", ch[4:8])[0]
l1 = struct.unpack(e + "%dI" % numl1, f.read(4 * numl1))
t = cyl * heads + head; n = 0
while True:
    f.seek(l1[t >> 8] + 8 * (t & 0xff)); pos, ln, sz = struct.unpack(e + "IHH", f.read(8))
    f.seek(pos); img = f.read(ln); body = img[5:]
    if img[0] & 1: body = zlib.decompress(body)
    elif img[0] & 2: body = bz2.decompress(body)
    p = 16
    while p + 8 <= len(body):
        cnt = body[p:p+8]
        if cnt == b"\xff" * 8: break
        cc, hh, r, kl, dl = struct.unpack(">HHBBH", cnt)
        if kl == 0 and dl == 0: print(f"dir blocks={n}, EOF at abs trk {t} R={r}"); sys.exit()
        n += 1; p += 8 + kl + dl
    t += 1
