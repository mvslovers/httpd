# usage: trkdump.py IMAGE CYL HEAD [NTRK] -- read-only: list the records (count fields) on CKD tracks of a CCKD image
import sys, struct, zlib, bz2
fn, cyl, head = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ntrk = int(sys.argv[4]) if len(sys.argv) > 4 else 1
f = open(fn, "rb")
dh = f.read(512)
assert dh[:8] in (b"CKD_C370", b"CKD_S370"), dh[:8]
heads, trksz = struct.unpack("<II", dh[8:16])
ch = f.read(512)
opts = ch[3]
e = ">" if opts & 0x02 else "<"
numl1 = struct.unpack(e + "I", ch[4:8])[0]
l1 = struct.unpack(e + "%dI" % numl1, f.read(4 * numl1))
print(f"heads={heads} trksz={trksz} numl1={numl1} opts={opts:#x}")
for t in range(cyl * heads + head, cyl * heads + head + ntrk):
    l2pos = l1[t >> 8]
    if l2pos in (0, 0xFFFFFFFF):
        print(f"trk {t}: null L2"); continue
    f.seek(l2pos + 8 * (t & 0xff))
    pos, ln, sz = struct.unpack(e + "IHH", f.read(8))
    if pos == 0:
        print(f"trk {t}: null track"); continue
    f.seek(pos); img = f.read(ln)
    comp = img[0]; body = img[5:]
    if comp & 0x01: body = zlib.decompress(body)
    elif comp & 0x02: body = bz2.decompress(body)
    c, h = struct.unpack(">HH", img[1:5])
    print(f"--- abs trk {t} = cyl {c} head {h} (comp {comp}, {ln} B on disk, {len(body)+5} B image)")
    p = 8 + 8  # skip R0 count + R0 data
    used = 0
    while p + 8 <= len(body):
        cnt = body[p:p+8]
        if cnt == b"\xff" * 8: print("    end of track"); break
        cc, hh, r, kl, dl = struct.unpack(">HHBBH", cnt)
        d = body[p+8+kl:p+8+kl+dl]
        print(f"    R={r:3d} (x{r:02X}) KL={kl} DL={dl:5d}  {d[:8].hex().upper()}")
        p += 8 + kl + dl
