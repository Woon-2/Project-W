using UnityEngine;
using UnityEditor;
using System.Collections.Generic;

[System.Serializable]
public class BoundingBoxData
{
    public string name;
    public Transform bone; // Bone reference
    public Vector3 localCenter = Vector3.zero;
    public Vector3 size = Vector3.one;
    public Vector3 rotationEuler = Vector3.zero;
}

[System.Serializable]
public class BoundingLOD
{
    public List<BoundingBoxData> boxes = new List<BoundingBoxData>();
}

public class MultiBoundingVolume : MonoBehaviour
{
    public int lodCount = 1;
    public List<BoundingLOD> lods = new List<BoundingLOD>();

    public void SyncLOD()
    {
        while (lods.Count < lodCount)
            lods.Add(new BoundingLOD());

        while (lods.Count > lodCount)
            lods.RemoveAt(lods.Count - 1);
    }
}