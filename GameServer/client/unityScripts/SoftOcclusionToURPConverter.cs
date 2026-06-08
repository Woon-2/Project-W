using UnityEngine;
using UnityEditor;

public static class SoftOcclusionToURPConverter
{
    [MenuItem("Tools/URP/Convert Soft Occlusion Materials")]
    public static void Convert()
    {
        Shader urpLit = Shader.Find("Universal Render Pipeline/Lit");

        if (urpLit == null)
        {
            Debug.LogError(
                "URP Lit shader not found. Is URP installed correctly?");
            return;
        }

        string[] guids = AssetDatabase.FindAssets("t:Material");

        int barkCount = 0;
        int leavesCount = 0;

        foreach (string guid in guids)
        {
            string path = AssetDatabase.GUIDToAssetPath(guid);
            Material mat = AssetDatabase.LoadAssetAtPath<Material>(path);

            if (mat == null || mat.shader == null)
                continue;

            string shaderName = mat.shader.name;

            bool isBark =
                shaderName == "Nature/Soft Occlusion Bark";

            bool isLeaves =
                shaderName == "Nature/Soft Occlusion Leaves";

            if (!isBark && !isLeaves)
                continue;

            Undo.RecordObject(mat, "Convert Tree Material");

            Color mainColor = Color.white;
            Texture mainTexture = null;
            float alphaCutoff = 0.4f;

            if (mat.HasProperty("_Color"))
                mainColor = mat.GetColor("_Color");

            if (mat.HasProperty("_MainTex"))
                mainTexture = mat.GetTexture("_MainTex");

            if (isLeaves && mat.HasProperty("_Cutoff"))
                alphaCutoff = mat.GetFloat("_Cutoff");

            mat.shader = urpLit;

            if (mat.HasProperty("_BaseColor"))
                mat.SetColor("_BaseColor", mainColor);

            if (mat.HasProperty("_BaseMap"))
                mat.SetTexture("_BaseMap", mainTexture);

            if (isLeaves)
            {
                // Alpha Clipping ON
                mat.SetFloat("_AlphaClip", 1.0f);

                if (mat.HasProperty("_Cutoff"))
                    mat.SetFloat("_Cutoff", alphaCutoff);

                // URP 내부 키워드 갱신
                mat.EnableKeyword("_ALPHATEST_ON");

                leavesCount++;
            }
            else
            {
                barkCount++;
            }

            EditorUtility.SetDirty(mat);
        }

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();

        Debug.Log(
            $"Soft Occlusion Conversion Complete\n" +
            $"Bark: {barkCount}\n" +
            $"Leaves: {leavesCount}");
    }
}