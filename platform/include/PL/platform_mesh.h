/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>
*/

#pragma once

#include <PL/platform_math.h>

typedef enum PLMeshPrimitive {
    PL_MESH_LINES,
    PL_MESH_LINE_STIPPLE,       /* todo */
    PL_MESH_LINE_LOOP,
    PL_MESH_LINE_STRIP,
    PL_MESH_POINTS,
    PL_MESH_TRIANGLES,
    PL_MESH_TRIANGLE_STRIP,
    PL_MESH_TRIANGLE_FAN,
    PL_MESH_TRIANGLE_FAN_LINE,
    PL_MESH_QUADS,
    PL_MESH_QUAD_STRIP,         /* todo */

    PL_NUM_PRIMITIVES
} PLMeshPrimitive;

typedef enum PLMeshDrawMode {
    PL_DRAW_STREAM,
    PL_DRAW_STATIC,
    PL_DRAW_DYNAMIC,

    PL_NUM_DRAWMODES
} PLMeshDrawMode;

typedef struct PLVertex {
    PLVector3       position, normal;
	PLVector3       tangent, bitangent;
    PLVector2       st[16];
    PLColour        colour;
    /* specific to skeletal animation */
    unsigned int    bone_index;
    float           bone_weight;
} PLVertex;

typedef struct PLTriangle {
    PLVector3       normal;
    unsigned int    indices[ 3 ];
} PLTriangle;

typedef struct PLTexture PLTexture;

typedef struct PLMesh {
	PLVertex        *vertices;
	unsigned int    num_verts;
	unsigned int    maxVertices;

	unsigned int    *indices;
    unsigned int    num_indices;
    unsigned int    maxIndices;
	unsigned int    num_triangles;

	struct PLShaderProgram* shader_program;
	PLTexture*              texture;
	PLMeshPrimitive         primitive;
	PLMeshDrawMode          mode;
    struct {
        unsigned int    buffers[32];
    } internal;
} PLMesh;

typedef struct PLCollisionAABB PLCollisionAABB;

PL_EXTERN_C

#if !defined( PL_COMPILE_PLUGIN )

PL_EXTERN PLMesh *plCreateMesh( PLMeshPrimitive primitive, PLMeshDrawMode mode, unsigned int num_tris, unsigned int num_verts );
PL_EXTERN PLMesh *plCreateMeshInit( PLMeshPrimitive primitive, PLMeshDrawMode mode, unsigned int numTriangles, unsigned int numVerts,
                                    const unsigned int *indicies, const PLVertex *vertices );
PL_EXTERN PLMesh *plCreateMeshRectangle( float x, float y, float w, float h, PLColour colour );
PL_EXTERN void plDestroyMesh( PLMesh *mesh );

PL_EXTERN void plDrawEllipse( unsigned int segments, PLVector2 position, float w, float h, PLColour colour );
PL_EXTERN void plDrawRectangle( const PLMatrix4 *transform, float x, float y, float w, float h, PLColour colour );
PL_EXTERN void plDrawTexturedRectangle( const PLMatrix4 *transform, float x, float y, float w, float h, PLTexture *texture );
PL_EXTERN void plDrawFilledRectangle( const PLRectangle2D *rectangle );
PL_EXTERN void plDrawTexturedQuad( const PLVector3 *ul, const PLVector3 *ur, const PLVector3 *ll, const PLVector3 *lr,
                                   float hScale, float vScale, PLTexture *texture );
PL_EXTERN void plDrawTriangle( int x, int y, unsigned int w, unsigned int h );
PL_EXTERN void plDrawLine( PLMatrix4 transform, PLVector3 startPos, PLColour startColour, PLVector3 endPos, PLColour endColour );
PL_EXTERN void plDrawSimpleLine( PLMatrix4 transform, PLVector3 startPos, PLVector3 endPos, PLColour colour );
PL_EXTERN void plDrawGrid( PLMatrix4 transform, int x, int y, int w, int h, unsigned int gridSize );
PL_EXTERN void plDrawMeshNormals( const PLMatrix4 *transform, const PLMesh *mesh );
PL_EXTERN void plDrawBoundingVolume( const PLCollisionAABB *bounds, PLColour colour );

PL_EXTERN void plClearMesh( PLMesh *mesh );
PL_EXTERN void plClearMeshVertices( PLMesh *mesh );
PL_EXTERN void plClearMeshTriangles( PLMesh *mesh );

PL_EXTERN void plScaleMesh( PLMesh *mesh, PLVector3 scale );
PL_EXTERN void plSetMeshTrianglePosition( PLMesh *mesh, unsigned int *index, unsigned int x, unsigned int y, unsigned int z );
PL_EXTERN void plSetMeshVertexPosition( PLMesh *mesh, unsigned int index, PLVector3 vector );
PL_EXTERN void plSetMeshVertexNormal( PLMesh *mesh, unsigned int index, PLVector3 vector );
PL_EXTERN void plSetMeshVertexST( PLMesh *mesh, unsigned int index, float s, float t );
PL_EXTERN void plSetMeshVertexSTv( PLMesh *mesh, uint8_t unit, unsigned int index, unsigned int size, const float *st );
PL_EXTERN void plSetMeshVertexColour( PLMesh *mesh, unsigned int index, PLColour colour );
PL_EXTERN void plSetMeshUniformColour( PLMesh *mesh, PLColour colour );
PL_EXTERN void plSetMeshShaderProgram( PLMesh *mesh, struct PLShaderProgram *program );

PL_EXTERN unsigned int plAddMeshVertex( PLMesh *mesh, PLVector3 position, PLVector3 normal, PLColour colour, PLVector2 st );
PL_EXTERN unsigned int plAddMeshTriangle( PLMesh *mesh, unsigned int x, unsigned int y, unsigned int z );

PL_EXTERN void plUploadMesh( PLMesh *mesh );
PL_EXTERN void plDrawMesh( PLMesh *mesh );

PL_EXTERN void plGenerateMeshNormals( PLMesh *mesh, bool perFace );
PL_EXTERN void plGenerateMeshTangentBasis( PLMesh *mesh );

PL_EXTERN void plGenerateTangentBasis( PLVertex *vertices, unsigned int numVertices, const unsigned int *indices, unsigned int numTriangles );
PL_EXTERN void plGenerateTextureCoordinates( PLVertex *vertices, unsigned int numVertices, PLVector2 textureOffset, PLVector2 textureScale );
PL_EXTERN void plGenerateVertexNormals( PLVertex *vertices, unsigned int numVertices, unsigned int *indices, unsigned int numTriangles, bool perFace );

PL_EXTERN PLVector3 plGenerateVertexNormal( PLVector3 a, PLVector3 b, PLVector3 c );

#endif

PL_EXTERN_C_END
