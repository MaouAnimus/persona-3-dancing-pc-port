using System;
using System.IO;
using System.Collections.Generic;
using System.Drawing.Imaging;
using System.Numerics;
using GFDLibrary;
using GFDLibrary.Models;
using GFDLibrary.Textures;

namespace ExportModel
{
    public struct Submesh
    {
        public uint TexId;
        public uint IndexStart;
        public uint IndexCount;
    }

    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("=================================================");
            Console.WriteLine("  Persona 3 Dancing — 3D Model Asset Exporter");
            Console.WriteLine("=================================================");

            string[] gmdFiles = new[]
            {
                "extracted_assets/dance/player/p3/pc101_01.GMD",     // Body
                "extracted_assets/dance/player/p3/face/pc101_f1.GMD", // Face
                "extracted_assets/dance/player/p3/hair/pc101_h00.GMD" // Hair
            };

            string stageGfsPath = "extracted_assets/dance/field/f101_001.GFS";
            if (File.Exists(stageGfsPath))
            {
                Console.WriteLine($"\nLoading Stage GFS: {stageGfsPath}");
                try
                {
                    var gfsRes = Resource.Load(stageGfsPath);
                    Console.WriteLine($"GFS loaded successfully! Type: {gfsRes.ResourceType}");
                    if (gfsRes is ModelPack gfsPack)
                    {
                        Console.WriteLine($"Stage ModelPack Meshes: {gfsPack.Model?.Meshes?.Count() ?? 0}, Textures: {gfsPack.Textures?.Count ?? 0}, Materials: {gfsPack.Materials?.Count ?? 0}");
                        Console.WriteLine($"Stage Bounding Box: {gfsPack.Model?.BoundingBox}");
                        if (gfsPack.Textures != null)
                        {
                            Directory.CreateDirectory("extracted_assets/stages/101");
                            int exported = 0;
                            foreach (var kvp in gfsPack.Textures)
                            {
                                string texName = kvp.Key;
                                var tex = kvp.Value;
                                try
                                {
                                    using var bmp = TextureDecoder.Decode(tex);
                                    string outPng = Path.Combine("extracted_assets/stages/101", Path.GetFileNameWithoutExtension(texName) + ".png");
                                    bmp.Save(outPng, ImageFormat.Png);
                                    exported++;
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"  [WARN] Failed to decode stage tex {texName}: {ex.Message}");
                                }
                            }
                            Console.WriteLine($"Successfully exported {exported}/{gfsPack.Textures.Count} stage textures to extracted_assets/stages/101!");
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ERROR] Failed to load GFS: {ex.Message}");
                }
            }

            string stageGmdPath = "extracted_assets/dance/field/object/m101_001.GMD";
            if (File.Exists(stageGmdPath))
            {
                Console.WriteLine($"\nLoading Stage GMD: {stageGmdPath}");
                try
                {
                    var gmdRes = Resource.Load(stageGmdPath);
                    Console.WriteLine($"Stage GMD loaded successfully! Type: {gmdRes.ResourceType}");
                    if (gmdRes is ModelPack stagePack)
                    {
                        Console.WriteLine($"Stage Object Meshes: {stagePack.Model?.Meshes?.Count() ?? 0}, Textures: {stagePack.Textures?.Count ?? 0}, Materials: {stagePack.Materials?.Count ?? 0}");
                        if (stagePack.Textures != null)
                        {
                            foreach (var tex in stagePack.Textures.Keys)
                                Console.WriteLine($"  Stage Obj Tex: {tex}");
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ERROR] Failed to load Stage GMD: {ex.Message}");
                }
            }

            string gapPath = "extracted_assets/dance/player/p3/101/pc101_001.GAP";
            if (File.Exists(gapPath))
            {
                Console.WriteLine($"\nLoading Animation GAP: {gapPath}");
                try
                {
                    var gapRes = Resource.Load(gapPath);
                    Console.WriteLine($"GAP loaded successfully! Type: {gapRes.ResourceType}");
                    if (gapRes is ModelPack gapPack && gapPack.AnimationPack != null)
                    {
                        var animPack = gapPack.AnimationPack;
                        Console.WriteLine($"GAP AnimationPack: Animations={animPack.Animations?.Count ?? 0}, BlendAnimations={animPack.BlendAnimations?.Count ?? 0}");
                        if (animPack.Animations != null)
                        {
                            for (int i = 0; i < animPack.Animations.Count; i++)
                            {
                                var anim = animPack.Animations[i];
                                Console.WriteLine($"  [Anim {i}] Duration={anim.Duration:F2}s, Controllers={anim.Controllers?.Count ?? 0}");
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ERROR] Failed to load GAP: {ex.Message}");
                }
            }

            Directory.CreateDirectory("extracted_assets/models/p3/pc101");
            string objPath = "extracted_assets/models/p3/pc101/makoto_yuki.obj";
            string binPath = "extracted_assets/models/p3/pc101/makoto_yuki.bin";

            using var objWriter = new StreamWriter(objPath);
            objWriter.WriteLine("# Makoto Yuki 3D Model for Persona 3 Dancing PC Native");

            using var binWriter = new BinaryWriter(File.Create(binPath));

            int globalVertexOffset = 1;
            int totalVertices = 0;
            int totalTriangles = 0;

            var allVertices = new List<float>();
            var allIndices = new List<uint>();

            var submeshes = new List<Submesh>();

            foreach (var gmdPath in gmdFiles)
            {
                if (!File.Exists(gmdPath))
                {
                    Console.WriteLine($"[WARN] File not found: {gmdPath}");
                    continue;
                }

                Console.WriteLine($"\nLoading GMD: {gmdPath}");
                var res = Resource.Load(gmdPath);

                Model? model = null;
                if (res is ModelPack pack)
                {
                    model = pack.Model;
                    Console.WriteLine($"ModelPack loaded! Model has {pack.Model?.Meshes?.Count() ?? 0} meshes.");

                    if (pack.Textures != null)
                    {
                        Console.WriteLine($"TextureDictionary has {pack.Textures.Count} textures. Exporting textures...");
                        foreach (var kvp in pack.Textures)
                        {
                            string texName = kvp.Key;
                            var tex = kvp.Value;
                            try
                            {
                                string texOut = Path.Combine("extracted_assets/models/p3/pc101", Path.GetFileNameWithoutExtension(texName) + ".png");
                                if (!File.Exists(texOut))
                                {
                                    using var bmp = TextureDecoder.Decode(tex);
                                    bmp.Save(texOut, ImageFormat.Png);
                                    Console.WriteLine($"  Exported texture: {texOut} ({bmp.Width}x{bmp.Height})");
                                }
                                else
                                {
                                    Console.WriteLine($"  Texture already exists: {texOut}");
                                }
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"  [WARN] Failed to export texture {texName}: {ex.Message}");
                            }
                        }
                    }
                }
                else if (res is Model m)
                {
                    model = m;
                }

                if (model == null)
                {
                    Console.WriteLine($"[WARN] Could not find Model in {gmdPath}");
                    continue;
                }

                Console.WriteLine($"Processing 3D Geometry: Bounding Box: {model.BoundingBox}");

                // Iterate through all meshes in the model
                foreach (var mesh in model.Meshes)
                {
                    if (mesh.Vertices == null || mesh.Vertices.Length == 0) continue;

                    string matName = mesh.MaterialName ?? "default";
                    string diffuseTex = "unknown";
                    if (res is ModelPack p && p.Materials != null && p.Materials.TryGetValue(matName, out var mat))
                    {
                        diffuseTex = mat.DiffuseMap?.Name ?? "none";
                    }

                    uint texId = 0;
                    if (diffuseTex.Contains("pc101_01_body00")) texId = 0;
                    else if (diffuseTex.Contains("pc101_f1_tex00")) texId = 1;
                    else if (diffuseTex.Contains("pc101_h00_tex00")) texId = 2;
                    else if (diffuseTex.Contains("pc101_e1_tex00")) texId = 3;
                    else if (matName.Contains("outline") || diffuseTex == "none") texId = 4;
                    else texId = 0;

                    Console.WriteLine($"  Mesh (V={mesh.Vertices.Length}, T={mesh.Triangles?.Length ?? 0}) | Mat '{matName}' -> Tex '{diffuseTex}' (Slot {texId})");
                    objWriter.WriteLine($"\no {matName}_{mesh.GetHashCode()}");

                    var verts = mesh.Vertices;
                    var norms = mesh.Normals;
                    var uvs = (mesh.TexCoordsChannel0 != null && mesh.TexCoordsChannel0.Length > 0) 
                              ? mesh.TexCoordsChannel0 
                              : null;

                    int localVertexStart = allVertices.Count / 12; // 12 floats per vertex

                    for (int i = 0; i < verts.Length; i++)
                    {
                        var pos = verts[i];
                        var norm = (norms != null && i < norms.Length) ? norms[i] : Vector3.UnitY;
                        var uv = (uvs != null && i < uvs.Length) ? uvs[i] : Vector2.Zero;

                        // OBJ format
                        objWriter.WriteLine($"v {pos.X:F4} {pos.Y:F4} {pos.Z:F4}");
                        objWriter.WriteLine($"vn {norm.X:F4} {norm.Y:F4} {norm.Z:F4}");
                        objWriter.WriteLine($"vt {uv.X:F4} {(1.0f - uv.Y):F4}");

                        // Binary format: pos(3), norm(3), uv(2), col(4)
                        allVertices.Add(pos.X);
                        allVertices.Add(pos.Y);
                        allVertices.Add(pos.Z);

                        allVertices.Add(norm.X);
                        allVertices.Add(norm.Y);
                        allVertices.Add(norm.Z);

                        allVertices.Add(uv.X);
                        allVertices.Add(uv.Y);

                        allVertices.Add(1.0f);
                        allVertices.Add(1.0f);
                        allVertices.Add(1.0f);
                        allVertices.Add(1.0f);
                    }

                    uint submeshIndexStart = (uint)allIndices.Count;

                    if (mesh.Triangles != null)
                    {
                        foreach (var tri in mesh.Triangles)
                        {
                            uint i0 = (uint)(localVertexStart + tri.A);
                            uint i1 = (uint)(localVertexStart + tri.B);
                            uint i2 = (uint)(localVertexStart + tri.C);

                            allIndices.Add(i0);
                            allIndices.Add(i1);
                            allIndices.Add(i2);

                            int a = globalVertexOffset + (int)tri.A;
                            int b = globalVertexOffset + (int)tri.B;
                            int c = globalVertexOffset + (int)tri.C;
                            objWriter.WriteLine($"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}");
                            totalTriangles++;
                        }
                    }

                    uint submeshIndexCount = (uint)allIndices.Count - submeshIndexStart;
                    if (submeshIndexCount > 0)
                    {
                        submeshes.Add(new Submesh { TexId = texId, IndexStart = submeshIndexStart, IndexCount = submeshIndexCount });
                    }

                    globalVertexOffset += verts.Length;
                    totalVertices += verts.Length;
                }
            }

            // Write binary model file in P3D2 format
            // Header: 'P3D2' (uint32), vertex_count (uint32), index_count (uint32), stride (uint32 = 48), submesh_count (uint32)
            binWriter.Write((byte)'P');
            binWriter.Write((byte)'3');
            binWriter.Write((byte)'D');
            binWriter.Write((byte)'2');
            binWriter.Write((uint)(allVertices.Count / 12));
            binWriter.Write((uint)allIndices.Count);
            binWriter.Write((uint)48); // 48 bytes per vertex: 3 pos + 3 norm + 2 uv + 4 col
            binWriter.Write((uint)submeshes.Count);

            foreach (var sm in submeshes)
            {
                binWriter.Write(sm.TexId);
                binWriter.Write(sm.IndexStart);
                binWriter.Write(sm.IndexCount);
            }

            foreach (float f in allVertices) binWriter.Write(f);
            foreach (uint idx in allIndices) binWriter.Write(idx);

            Console.WriteLine("\n=================================================");
            Console.WriteLine($"EXPORT COMPLETE!");
            Console.WriteLine($"  Total Vertices:  {totalVertices}");
            Console.WriteLine($"  Total Triangles: {totalTriangles}");
            Console.WriteLine($"  Saved OBJ:       {objPath}");
            Console.WriteLine($"  Saved Binary:    {binPath}");
            Console.WriteLine("=================================================");
        }
    }
}
