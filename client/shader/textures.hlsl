// - MAT_SPACE
// Above is the texture space used in the shader.
// It is regarded as macros in the shader, and must be passed in as a define when compiling the shader.

Texture2D gTex2DLUT[] : register(t1, MAT_SPACE);
TextureCube gTexCubeLUT[] : register(t1, MAT_SPACE);