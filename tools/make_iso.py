import os
import sys
import struct
import datetime

SECTOR_SIZE = 2048

def to_both_endian_16(val):
    return struct.pack('<H', val) + struct.pack('>H', val)

def to_both_endian_32(val):
    return struct.pack('<I', val) + struct.pack('>I', val)

def format_date_recording(dt):
    return struct.pack('BBBBBBb', dt.year - 1900, dt.month, dt.day, dt.hour, dt.minute, dt.second, 0)

def format_date_volume(dt):
    s = f"{dt.year:04d}{dt.month:02d}{dt.day:02d}{dt.hour:02d}{dt.minute:02d}{dt.second:02d}00"
    return s.encode('ascii') + struct.pack('b', 0)

def make_dir_record(lba, size, flags, name_bytes, dt):
    name_len = len(name_bytes)
    rec_len = 33 + name_len
    if rec_len % 2 != 0:
        rec_len += 1
    pad = b'\x00' * (rec_len - (33 + name_len))
    
    buf = struct.pack('BB', rec_len, 0)
    buf += to_both_endian_32(lba)
    buf += to_both_endian_32(size)
    buf += format_date_recording(dt)
    buf += struct.pack('BBB', flags, 0, 0)
    buf += to_both_endian_16(1)
    buf += struct.pack('B', name_len)
    buf += name_bytes + pad
    return buf

def build_iso(src_dir, output_iso):
    now = datetime.datetime.utcnow()
    
    file_list = []
    for root, dirs, files in os.walk(src_dir):
        if '.git' in root.split(os.sep) or 'tools' in root.split(os.sep):
            continue
        for f in files:
            if f.endswith('.iso') or f.endswith('.img'):
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, src_dir)
            file_list.append((rel, full))
            
    file_list.sort(key=lambda x: x[0].upper())
    
    file_records = []
    cur_sector = 20
    
    for rel, full in file_list:
        with open(full, 'rb') as fp:
            data = fp.read()
        size = len(data)
        lba = cur_sector
        sectors_needed = (size + SECTOR_SIZE - 1) // SECTOR_SIZE
        if sectors_needed == 0:
            sectors_needed = 1
        cur_sector += sectors_needed
        
        base = os.path.basename(rel).upper().replace('-', '_').replace(' ', '_')
        if '.' in base:
            parts = base.split('.')
            name = parts[0][:8] + '.' + parts[1][:3]
        else:
            name = base[:8]
        iso_name = (name + ';1').encode('ascii')
        
        file_records.append({
            'lba': lba,
            'size': size,
            'name_bytes': iso_name,
            'data': data,
            'orig': rel
        })

    total_sectors = cur_sector
    
    root_lba = 19
    dot_rec = make_dir_record(root_lba, SECTOR_SIZE, 0x02, b'\x00', now)
    dotdot_rec = make_dir_record(root_lba, SECTOR_SIZE, 0x02, b'\x01', now)
    
    root_dir_data = bytearray(dot_rec + dotdot_rec)
    for rec in file_records:
        r = make_dir_record(rec['lba'], rec['size'], 0x00, rec['name_bytes'], now)
        root_dir_data.extend(r)
        
    root_dir_sector = bytes(root_dir_data).ljust(SECTOR_SIZE, b'\x00')
    
    pvd = bytearray(SECTOR_SIZE)
    pvd[0] = 0x01
    pvd[1:6] = b'CD001'
    pvd[6] = 0x01
    pvd[8:40] = b'ARENAOS'.ljust(32, b' ')
    pvd[40:72] = b'NT31_TERMINAL'.ljust(32, b' ')
    pvd[80:88] = to_both_endian_32(total_sectors)
    pvd[120:124] = to_both_endian_16(1)
    pvd[124:128] = to_both_endian_16(1)
    pvd[128:132] = to_both_endian_16(SECTOR_SIZE)
    pvd[132:140] = to_both_endian_32(10)
    pvd[140:144] = struct.pack('<I', 18)
    pvd[148:152] = struct.pack('>I', 18)
    
    root_dr = make_dir_record(root_lba, SECTOR_SIZE, 0x02, b'\x00', now)
    pvd[156:156+len(root_dr)] = root_dr
    
    pvd[190:318] = b'NT31_DISTRIBUTION'.ljust(128, b' ')
    pvd[318:446] = b'ARENA_USER'.ljust(128, b' ')
    pvd[446:574] = b'ARENAOS_PROJECT'.ljust(128, b' ')
    pvd[574:702] = b'NT31_BUILD_SYSTEM'.ljust(128, b' ')
    pvd[702:739] = b'README.MD'.ljust(37, b' ')
    pvd[739:776] = b''.ljust(37, b' ')
    pvd[776:813] = b''.ljust(37, b' ')
    pvd[813:830] = format_date_volume(now)
    pvd[830:847] = format_date_volume(now)
    pvd[847:864] = b'0000000000000000\x00'
    pvd[864:881] = format_date_volume(now)
    pvd[881] = 0x01
    
    term = bytearray(SECTOR_SIZE)
    term[0] = 0xFF
    term[1:6] = b'CD001'
    term[6] = 0x01
    
    pt = bytearray(SECTOR_SIZE)
    pt[0:10] = struct.pack('<BBIH', 1, 0, root_lba, 1) + b'\x00\x00'
    
    with open(output_iso, 'wb') as out:
        out.write(b'\x00' * (16 * SECTOR_SIZE))
        out.write(pvd)
        out.write(term)
        out.write(pt)
        out.write(root_dir_sector)
        for rec in file_records:
            out.seek(rec['lba'] * SECTOR_SIZE)
            out.write(rec['data'])
            rem = len(rec['data']) % SECTOR_SIZE
            if rem != 0:
                out.write(b'\x00' * (SECTOR_SIZE - rem))
                
        out.seek(total_sectors * SECTOR_SIZE)
        out.truncate()

    print(f"ISO generated: {output_iso} ({total_sectors * SECTOR_SIZE} bytes, {len(file_records)} files)")

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else '.'
    dst = sys.argv[2] if len(sys.argv) > 2 else 'nt31.iso'
    build_iso(src, dst)
