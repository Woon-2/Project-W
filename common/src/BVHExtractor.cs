using System.Text;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public class BVHExtractor : MonoBehaviour
{
    private const int BOX_COLLIDER = 0;
    private const int CAPSULE_COLLIDER = 1;

    private BinaryWriter binaryWriter = null;
    private Queue<GameObject> jobQueue = null;
    private int lastLevel = -1;
    private List<int> levelNodeCnts = null;

    void Start()
    {
        binaryWriter = new BinaryWriter(File.Open(string.Copy(transform.parent.gameObject.name).Replace(" ", "_") + ".bvh", FileMode.Create));
        jobQueue = new Queue<GameObject>();
        levelNodeCnts = new List<int>();
        ExtractBVH(transform);
    }

    void ExtractBVH(Transform transform)
    {
        GameObject obj;
        int curLevelNodeCnt = 0;

        // calcaulate level nodes count to reserve node when import
        jobQueue.Enqueue(transform.gameObject);
        while (jobQueue.TryDequeue(out obj))
        {
            int level = GetLevel(obj);
            if (level > lastLevel)
            {
                if (level > 0)
                {
                    levelNodeCnts.Add(curLevelNodeCnt);
                }
                curLevelNodeCnt = 0;
                lastLevel = level;
            }

            ++curLevelNodeCnt;

            for (int k = 0; k < obj.transform.childCount; k++)
            {
                jobQueue.Enqueue(obj.transform.GetChild(k).gameObject);
            }
        }

        levelNodeCnts.Add(curLevelNodeCnt);
        lastLevel = -1;

        // actual extraction
        jobQueue.Enqueue(transform.gameObject);
        while (jobQueue.TryDequeue(out obj))
        {
            int level = GetLevel(obj);
            if (level > lastLevel)
            {
                WriteInteger("<Level:>", level);
                WriteInteger(levelNodeCnts[level]);
                lastLevel = level;
            }

            ExtractColliderInfo(obj);

            for (int k = 0; k < obj.transform.childCount; k++)
            {
                jobQueue.Enqueue(obj.transform.GetChild(k).gameObject);
            }
        }
    }

    void ExtractColliderInfo(GameObject obj)
    {
        WriteString("<Node:>");
        foreach (var collider in obj.GetComponents<Collider>())
        {
            WriteString("<Collider:>");
            // sb.AppendLine($"GameObject: {collider.gameObject.name}");
            WriteString("<Name:>", obj.name);

            if (collider is BoxCollider box)
            {
                Vector3 center = box.transform.TransformPoint(box.center);
                Vector3 size = Vector3.Scale(box.transform.localScale, box.size);

                WriteInteger("<Type:>", BOX_COLLIDER);
                WriteVector("<Center:>", center);
                WriteVector("<Extents:>", size * 0.5f);

                // OBB 정보 (객체 회전 고려)
                Quaternion rotation = box.transform.rotation;
                WriteVector("<Orientation:>", rotation);
            }
            else if (collider is CapsuleCollider capsule)
            {
                WriteInteger("<Type:>", CAPSULE_COLLIDER);

                // Capsule Collider의 방향 계산
                Vector3 directionVector = Vector3.zero;
                switch (capsule.direction)
                {
                    case 0: directionVector = Vector3.right; break; // X 방향
                    case 1: directionVector = Vector3.up; break;   // Y 방향
                    case 2: directionVector = Vector3.forward; break; // Z 방향
                }

                // Capsule의 Base와 Tip을 계산 (로컬 좌표계에서)
                Vector3 localBasePosition = capsule.center - directionVector * capsule.height / 2;
                Vector3 localTipPosition = capsule.center + directionVector * capsule.height / 2;

                // 로컬 좌표계에서 월드 좌표계로 변환
                Vector3 worldBasePosition = capsule.transform.TransformPoint(localBasePosition);
                Vector3 worldTipPosition = capsule.transform.TransformPoint(localTipPosition);

                float radius = capsule.transform.localScale.x * capsule.radius;

                WriteVector("<Base:>", worldBasePosition);
                WriteVector("<Tip:>", worldTipPosition);
                WriteFloat("<Radius:>", radius);
            }
            WriteString("</Collider>");
        }
        WriteString("</Node>");
    }

    int GetLevel(GameObject obj)
    {
        return obj.name.IndexOf('_') - 2;
    }

    void WriteInteger(int i)
    {
        binaryWriter.Write(i);
    }

    void WriteInteger(string strHeader, int i)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(i);
    }

    void WriteFloat(string strHeader, float f)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(f);
    }

    void WriteVector(Vector2 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
    }

    void WriteVector(string strHeader, Vector2 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    void WriteVector(Vector3 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
    }

    void WriteVector(string strHeader, Vector3 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    void WriteVector(Vector4 v)
    {
        binaryWriter.Write(v.x);
        binaryWriter.Write(v.y);
        binaryWriter.Write(v.z);
        binaryWriter.Write(v.w);
    }

    void WriteVector(string strHeader, Vector4 v)
    {
        binaryWriter.Write(strHeader);
        WriteVector(v);
    }

    void WriteVector(Quaternion q)
    {
        binaryWriter.Write(q.x);
        binaryWriter.Write(q.y);
        binaryWriter.Write(q.z);
        binaryWriter.Write(q.w);
    }

    void WriteVector(string strHeader, Quaternion q)
    {
        binaryWriter.Write(strHeader);
        WriteVector(q);
    }

    void WriteString(string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    void WriteString(string strHeader, string strToWrite)
    {
        binaryWriter.Write(strHeader);
        binaryWriter.Write(strToWrite);
    }
}