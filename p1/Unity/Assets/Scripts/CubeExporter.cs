using UnityEngine;
using System.IO;

public class CubeExporter : ILevelExportable
{
    public override void ExportLevelNodeData(BinaryWriter binaryWriter, Transform root)
    {
        ExportLevelNodeHeader(binaryWriter, root);

        ExtractUtil.WriteText(binaryWriter, "Mesh", "CubeMesh");

        MaterialSetSelector materialSetSelector = gameObject.GetComponent<MaterialSetSelector>();
        if (materialSetSelector != null)
        {
            ExtractUtil.WriteText( binaryWriter, "MaterialSet",
                materialSetSelector.materialSets[materialSetSelector.currentSetIndex].name
            );
        }
        else
        {
            Debug.LogWarning(gameObject.name + " doesn't have MaterialSetSelector Component, "
                + "the object will be exported without material set info.");
        }

        ExportChildren(binaryWriter, root);

        ExportLevelNodeFooter(binaryWriter, root);
    }
}
