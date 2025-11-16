using System.Collections.Generic;
using UnityEngine;

[ExecuteAlways] // 에디터에서도 동작
public class SkyboxSelector : MonoBehaviour
{
    [Tooltip("등록된 스카이박스 재질들")]
    public List<Material> skyboxMaterials = new List<Material>();

    [Tooltip("현재 적용 중인 스카이박스 인덱스")]
    public int currentMaterialIdx = 0;

    private Renderer targetRenderer;

    void OnValidate()
    {
        // 이미 적용된 스카이박스 재질을 자동으로 0번 재질로 등록
        TryAddDefaultSkybox();
        ApplyCurrentSkybox();
    }

    private void TryAddDefaultSkybox()
    {
        var currMaterial = RenderSettings.skybox;

        if (currMaterial == null)
        {
            return;
        }

        if (skyboxMaterials.Count == 0)
        {
            skyboxMaterials.Add(currMaterial);
        }
    }

    public void ApplyCurrentSkybox()
    {
        if (skyboxMaterials.Count == 0)
            return;

        currentMaterialIdx = Mathf.Clamp(currentMaterialIdx, 0, skyboxMaterials.Count - 1);
        var material = skyboxMaterials[currentMaterialIdx];

        RenderSettings.skybox = material;
    }
}