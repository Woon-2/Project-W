using UnityEngine;
using System.IO;

[System.Serializable]
public abstract class ILevelExportable : MonoBehaviour
{
    public string type;

    protected void ExportLevelNodeHeader(BinaryWriter binaryWriter, Transform root)
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "Node");
        ExtractUtil.WriteText(binaryWriter, "Type", type);
        ExtractUtil.WriteText(binaryWriter, "Name", gameObject.name);
        ExtractUtil.WriteLocalMatrix(binaryWriter, "LocalMatrix", transform);
        ExtractUtil.WriteDressMatrix(binaryWriter, "WorldMatrix", root, transform);
    }

    protected void ExportLevelNodeFooter(BinaryWriter binaryWriter, Transform root)
    {
        ExtractUtil.WriteTailTag(binaryWriter, "Node");
    }

    protected void ExportChildren(BinaryWriter binaryWriter, Transform root)
    {
        ExtractUtil.WriteInteger(binaryWriter, "ChildCnt", transform.childCount);
        ExtractUtil.WriteHeadTag(binaryWriter, "Children");

        for (int k = 0; k < transform.childCount; k++)
        {
            transform.GetChild(k).GetComponent<ILevelExportable>().ExportLevelNodeData(binaryWriter, root);
        }

        ExtractUtil.WriteTailTag(binaryWriter, "Children");
    }

    public abstract void ExportLevelNodeData(BinaryWriter binaryWriter, Transform root);
}
