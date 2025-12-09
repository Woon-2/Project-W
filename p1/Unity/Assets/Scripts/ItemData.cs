using UnityEngine;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;

[CreateAssetMenu(fileName = "ItemData", menuName = "Game/Item Data")]
public class ItemData : ScriptableObject
{
    public List<SocketOffset> socketOffsets = new List<SocketOffset>();

    // 특정 소켓에서 사용할 offset 반환
    public SocketOffset GetOffset(SocketType type)
    {
        return socketOffsets.Find(x => x.socketType == type);
    }

    public void WriteBinaryInfo(BinaryWriter binaryWriter)
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "ItemData");
        ExtractUtil.WriteHeadTag(binaryWriter, "SocketOffsets");
        ExtractUtil.WriteInteger(binaryWriter, "SocketOffsetCnt", socketOffsets.Count);
        for (int i = 0; i < socketOffsets.Count; ++i)
        {
            ExtractUtil.WriteHeadTag(binaryWriter, "SocketOffset");
            ExtractUtil.WriteText(binaryWriter, "SocketType", socketOffsets[i].socketType.ToString());
            ExtractUtil.WriteVector(binaryWriter, "Translation", socketOffsets[i].translation);
            ExtractUtil.WriteQuaternion(binaryWriter, "Rotation", Quaternion.Euler(socketOffsets[i].rotation));
            ExtractUtil.WriteVector(binaryWriter, "Scale", socketOffsets[i].scale);
            ExtractUtil.WriteTailTag(binaryWriter, "SocketOffset");
        }
        ExtractUtil.WriteTailTag(binaryWriter, "SocketOffsets");

        ExtractUtil.WriteTailTag(binaryWriter, "ItemData");
    }
}