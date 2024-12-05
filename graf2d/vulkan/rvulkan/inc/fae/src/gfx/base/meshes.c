typedef struct Vertex {
  V3 pos;
  V2 uv;
} Vertex;

// ---------------------------------------------------------------------
//                            CUBE
// ---------------------------------------------------------------------
internal
void get_cube_vertices(Vertex vertices[36])
{
 Vertex cube_positions[] = {
   (Vertex) { .pos = (V3) { -0.5f, -0.5f, -0.5f } },
   (Vertex) { .pos = (V3) {  0.5f,  0.5f, -0.5f } },
   (Vertex) { .pos = (V3) {  0.5f, -0.5f, -0.5f } },

   (Vertex) { .pos = (V3) {  0.5f,  0.5f, -0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f, -0.5f } },
   (Vertex) { .pos = (V3) { -0.5f,  0.5f, -0.5f } },

   (Vertex) { .pos = (V3) { -0.5f,  0.5f, -0.5f } },
   (Vertex) { .pos = (V3) {  0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) {  0.5f,  0.5f, -0.5f } },

   (Vertex) { .pos = (V3) {  0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f,  0.5f, -0.5f } },
   (Vertex) { .pos = (V3) { -0.5f,  0.5f,  0.5f } },

   (Vertex) { .pos = (V3) {  0.5f,  0.5f, -0.5f } },
   (Vertex) { .pos = (V3) {  0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) {  0.5f, -0.5f, -0.5f } },

   (Vertex) { .pos = (V3) {  0.5f, -0.5f, -0.5f } },
   (Vertex) { .pos = (V3) {  0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) {  0.5f, -0.5f,  0.5f } },

   (Vertex) { .pos = (V3) {  0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f,  0.5f } },
   (Vertex) { .pos = (V3) {  0.5f, -0.5f,  0.5f } },

   (Vertex) { .pos = (V3) { -0.5f, -0.5f,  0.5f } },
   (Vertex) { .pos = (V3) {  0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f,  0.5f,  0.5f } },

   (Vertex) { .pos = (V3) { -0.5f, -0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f,  0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f,  0.5f, -0.5f } },

   (Vertex) { .pos = (V3) { -0.5f,  0.5f, -0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f, -0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f,  0.5f } },

   (Vertex) { .pos = (V3) {  0.5f, -0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f, -0.5f } },
   (Vertex) { .pos = (V3) {  0.5f, -0.5f, -0.5f } },

   (Vertex) { .pos = (V3) {  0.5f, -0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f,  0.5f } },
   (Vertex) { .pos = (V3) { -0.5f, -0.5f, -0.5f } },
 };

  memcpy(vertices, cube_positions, sizeof(cube_positions));
}
