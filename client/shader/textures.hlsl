// - TEX2D_SPACE
// - TEXCUBE_SPACE
// Above are the texture spaces used in the shader.
// They are regarded as macros in the shader, and must be passed in as a define when compiling the shader.

Texture2D gTex2DLUT[] : register(t1, TEX2D_SPACE);
TextureCube gTexCubeLUT[] : register(t1, TEXCUBE_SPACE);