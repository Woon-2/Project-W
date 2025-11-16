using UnityEngine;
using System.IO;

public class DefaultExporter : ILevelExportable
{
    public override void ExportLevelNodeData(BinaryWriter binaryWriter, Transform root)
    {
        ExportLevelNodeHeader(binaryWriter, root);

        // 자식 노드들 추출
        ExportChildren(binaryWriter, root);

        ExportLevelNodeFooter(binaryWriter, root);
    }
}
