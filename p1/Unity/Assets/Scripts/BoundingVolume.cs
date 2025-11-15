using UnityEngine;
using System;
using System.Collections;
using System.Collections.Generic;

public class BoundingVolume : MonoBehaviour
{
    // --- Cubemap 정보 구조 ---
    [Serializable]
    public class BVInfo
    {
        public string BVName;
        public BoxCollider BVBox;
    }

    public string volumeName = "Volume";
    public List<BVInfo> boundingVolumes = new List<BVInfo>();
}