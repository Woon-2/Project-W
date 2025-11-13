using System.Collections.Generic;
using UnityEngine;

[System.Serializable]
public class MaterialSet
{
    public string name = "Set";
    public Material[] materials;
}

[ExecuteAlways] // 에디터에서도 동작
[RequireComponent(typeof(MeshRenderer))]
public class MaterialSetSelector : MonoBehaviour
{
    [Tooltip("등록된 머티리얼 세트들")]
    public List<MaterialSet> materialSets = new List<MaterialSet>();

    [Tooltip("현재 적용 중인 세트 인덱스")]
    public int currentSetIndex = 0;

    private MeshRenderer meshRenderer;

    void OnValidate()
    {
        meshRenderer = GetComponent<MeshRenderer>();
        ApplyCurrentSet();
    }

    public void ApplyCurrentSet()
    {
        if (meshRenderer == null || materialSets.Count == 0)
            return;

        currentSetIndex = Mathf.Clamp(currentSetIndex, 0, materialSets.Count - 1);
        var set = materialSets[currentSetIndex];

        if (set.materials != null && set.materials.Length > 0)
            meshRenderer.sharedMaterials = set.materials;
    }
}