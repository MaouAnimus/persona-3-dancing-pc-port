import struct

class BinaryStream:
    def __init__(self, data, pos=0):
        self.data = data
        self.pos = pos

    def read_u8(self):
        val = self.data[self.pos]
        self.pos += 1
        return val

    def read_u16_be(self):
        val = struct.unpack('>H', self.data[self.pos:self.pos+2])[0]
        self.pos += 2
        return val

    def read_i16_be(self):
        val = struct.unpack('>h', self.data[self.pos:self.pos+2])[0]
        self.pos += 2
        return val

    def read_u32_be(self):
        val = struct.unpack('>I', self.data[self.pos:self.pos+4])[0]
        self.pos += 4
        return val

    def read_i32_be(self):
        val = struct.unpack('>i', self.data[self.pos:self.pos+4])[0]
        self.pos += 4
        return val

    def read_f32_be(self):
        val = struct.unpack('>f', self.data[self.pos:self.pos+4])[0]
        self.pos += 4
        return val

    def read_vec2_be(self):
        v = struct.unpack('>ff', self.data[self.pos:self.pos+8])
        self.pos += 8
        return v

    def read_vec3_be(self):
        v = struct.unpack('>fff', self.data[self.pos:self.pos+12])
        self.pos += 12
        return v

    def read_vec4_be(self):
        v = struct.unpack('>ffff', self.data[self.pos:self.pos+16])
        self.pos += 16
        return v

    def read_string(self):
        str_len = self.read_u16_be()
        if str_len == 0:
            return ""
        s = self.data[self.pos:self.pos+str_len].decode('shift_jis', errors='ignore')
        self.pos += str_len
        return s

    def read_string_with_hash(self, version=0x01105090):
        s = self.read_string()
        if len(s) > 0 and version > 0x1080000:
            self.read_u32_be() # hash
        return s

def read_mesh(s, version):
    # Resource header: 16 bytes
    magic = s.read_u32_be()
    m_type = s.read_u32_be()
    m_size = s.read_u32_be()
    m_pad = s.read_u32_be()

    flags = s.read_i32_be()
    attr_flags = s.read_i32_be()

    has_triangles = (flags & 1) != 0
    has_mat = (flags & 2) != 0
    has_weights = (flags & 8) != 0
    has_bbox = (flags & 0x10) != 0
    has_bsphere = (flags & 0x20) != 0

    triangle_count = 0
    triangles = []
    idx_format = 0
    if has_triangles:
        triangle_count = s.read_i32_be()
        idx_format = s.read_i16_be()

    vertex_count = s.read_i32_be()
    if version > 0x1103020:
        field14 = s.read_i32_be()

    has_pos = (attr_flags & 1) != 0
    has_norm = (attr_flags & 2) != 0
    has_tan = (attr_flags & 4) != 0
    has_binorm = (attr_flags & 8) != 0
    has_col0 = (attr_flags & 0x10) != 0
    has_uv0 = (attr_flags & 0x20) != 0
    has_uv1 = (attr_flags & 0x40) != 0
    has_uv2 = (attr_flags & 0x80) != 0
    has_col1 = (attr_flags & 0x100) != 0

    vertices = []
    normals = []
    uvs = []

    for _ in range(vertex_count):
        if has_pos:
            vertices.append(s.read_vec3_be())
        if has_norm:
            normals.append(s.read_vec3_be())
        if has_tan:
            s.read_vec3_be()
        if has_binorm:
            s.read_vec3_be()
        if has_col0:
            s.read_u32_be()
        if has_uv0:
            uvs.append(s.read_vec2_be())
        if has_uv1:
            s.read_vec2_be()
        if has_uv2:
            s.read_vec2_be()
        if has_col1:
            s.read_u32_be()
        if has_weights:
            s.pos += 16 + 4

    if has_triangles:
        for _ in range(triangle_count):
            if idx_format == 0: # uint16
                triangles.append((s.read_u16_be(), s.read_u16_be(), s.read_u16_be()))
            else: # uint32
                triangles.append((s.read_u32_be(), s.read_u32_be(), s.read_u32_be()))

    mat_name = ""
    if has_mat:
        mat_name = s.read_string_with_hash(version)

    if has_bbox:
        s.read_vec3_be(); s.read_vec3_be()
    if has_bsphere:
        s.read_vec3_be(); s.read_f32_be()

    return {
        'mat_name': mat_name,
        'vertices': vertices,
        'normals': normals,
        'uvs': uvs,
        'triangles': triangles
    }

def read_node(s, version, depth=0):
    # Resource header: 16 bytes
    magic = s.read_u32_be()
    n_type = s.read_u32_be()
    n_size = s.read_u32_be()
    n_pad = s.read_u32_be()

    name = s.read_string_with_hash(version)
    trans = s.read_vec3_be()
    rot = s.read_vec4_be()
    scale = s.read_vec3_be()

    attach_count = s.read_i32_be()
    meshes = []
    for _ in range(attach_count):
        attach_type = s.read_i32_be()
        if attach_type == 2: # NodeAttachmentType.Mesh
            m = read_mesh(s, version)
            meshes.append(m)
        elif attach_type == 1: # NodeAttachmentType.Node
            read_node(s, version, depth + 1)
        else:
            print(f"Attachment type {attach_type} encountered")

    # Properties
    if version > 0x1060000:
        prop_count = s.read_i32_be()
        for _ in range(prop_count):
            p_name = s.read_string_with_hash(version)
            p_type = s.read_i32_be()
            if p_type == 0: s.read_i32_be()
            elif p_type == 1: s.read_f32_be()
            elif p_type == 2: s.read_u8()
            elif p_type == 3: s.read_string_with_hash(version)

    child_count = s.read_i32_be()
    children = []
    for _ in range(child_count):
        child = read_node(s, version, depth + 1)
        children.append(child)

    return {
        'name': name,
        'translation': trans,
        'rotation': rot,
        'scale': scale,
        'meshes': meshes,
        'children': children
    }

def main():
    path = 'extracted_assets/dance/player/p3/pc101_01.GMD'
    with open(path, 'rb') as f:
        data = f.read()

    magic = b'\x01\x10\x50\x90\x00\x01\x00\x03'
    idx = data.find(magic)
    s = BinaryStream(data, idx)

    version = 0x01105090
    s.read_u32_be() # magic
    s.read_u32_be() # type
    s.read_u32_be() # size
    s.read_u32_be() # pad

    flags = s.read_i32_be()
    if (flags & 1) != 0:
        bone_count = s.read_i32_be()
        s.pos += bone_count * 64 + bone_count * 2
    if (flags & 2) != 0:
        s.read_vec3_be(); s.read_vec3_be()
    if (flags & 4) != 0:
        s.read_vec3_be(); s.read_f32_be()

    print("Parsing RootNode hierarchy...")
    root = read_node(s, version)
    
    total_verts = 0
    total_tris = 0
    def print_tree(node, indent=0):
        nonlocal total_verts, total_tris
        pad = "  " * indent
        mesh_info = ""
        if node['meshes']:
            for m in node['meshes']:
                v_cnt = len(m['vertices'])
                t_cnt = len(m['triangles'])
                total_verts += v_cnt
                total_tris += t_cnt
                mesh_info += f" [MESH: '{m['mat_name']}', {v_cnt} verts, {t_cnt} tris]"
        if mesh_info or indent <= 3:
            print(f"{pad}- {node['name']}{mesh_info}")
        for c in node['children']:
            print_tree(c, indent + 1)

    print_tree(root)
    print(f"\n=======================================================")
    print(f"SUCCESS! Parsed Makoto Yuki 3D Model:")
    print(f"  Total Vertices:  {total_verts}")
    print(f"  Total Triangles: {total_tris}")
    print(f"=======================================================")

if __name__ == '__main__':
    main()
