using System.Collections.Generic;
using UnityEngine;

[System.Serializable]
public class MaterialSet
{
    public string name = "Set";
    public Material[] materials;
}

[ExecuteAlways] // 에디터에서도 동작
[RequireComponent(typeof(Renderer))]
public class MaterialSetSelector : MonoBehaviour
{
    [Tooltip("등록된 머티리얼 세트들")]
    public List<MaterialSet> materialSets = new List<MaterialSet>();

    [Tooltip("현재 적용 중인 세트 인덱스")]
    public int currentSetIndex = 0;

    private Renderer targetRenderer;

    void OnValidate()
    {
        targetRenderer = GetComponent<Renderer>();
        // 이미 적용된 머티리얼 세트를 자동으로 0번 세트로 등록
        TryAddDefaultMaterialSet();
        ApplyCurrentSet();
    }

    private void TryAddDefaultMaterialSet()
    {
        if (targetRenderer == null)
            return;

        var currentMats = targetRenderer.sharedMaterials;

        if (currentMats == null || currentMats.Length == 0)
            return;

        // 0번 세트가 없으면 새로 추가
        if (materialSets.Count == 0)
        {
            materialSets.Add(new MaterialSet
            {
                name = "Default (From Renderer)",
                materials = currentMats
            });
        }
        else
        {
            var firstSet = materialSets[0];

            // 0번 세트가 비어 있거나 길이가 다르면 덮어쓰기
            if (firstSet.materials == null ||
                firstSet.materials.Length != currentMats.Length)
            {
                firstSet.name = "Default (From Renderer)";
                firstSet.materials = currentMats;
            }
        }
    }

    public void ApplyCurrentSet()
    {
        if (targetRenderer == null || materialSets.Count == 0)
            return;

        currentSetIndex = Mathf.Clamp(currentSetIndex, 0, materialSets.Count - 1);
        var set = materialSets[currentSetIndex];

        if (set.materials != null && set.materials.Length > 0)
            targetRenderer.sharedMaterials = set.materials;
    }
}