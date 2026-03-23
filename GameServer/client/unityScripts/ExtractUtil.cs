using UnityEngine;
using System.IO;

public static class ExtractUtil
{
    public static string ReplaceWhiteSpace(string str)
    {
        return string.Copy(str).Replace(" ", "_");
    }

    public static string MakeHeadTag(string tagSource)
    {
        return string.Format("<{0}:>", tagSource);
    }

    public static string MakeTailTag(string tagSource)
    {
        return string.Format("</{0}>", tagSource);
    }

    // WriteXX...: BinaryWriter로 특정한 자료형을 출력하기 위한 유틸리티 함수
    // ExtractXX...: WriteXX... 함수들을 이용하여 유니티의 특정한 자료구조를 출력하는 함수

    // @param Transform 노드 개수 추적의 대상이 될 트리(서브트리)의 루트노드
    // @param nodeCnt 노드 개수를 누적시킬 변수
    // Transform 트리(서브트리)의 노드 개수를 nodeCnt에 누적한다.
    public static void AccNodeCnt(Transform xform, ref int nodeCnt)
    {
        nodeCnt++;
        if (xform.childCount > 0)
        {
            for (int k = 0; k < xform.childCount; k++)
            {
                AccNodeCnt(xform.GetChild(k), ref nodeCnt);
            }
        }
    }

    public static void WriteString(BinaryWriter binaryWriter, string strToWrite)
    {
        binaryWriter.Write(strToWrite);
    }

    public static void WriteText(BinaryWriter binaryWriter, string tagSource, string text)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteString(binaryWriter, text);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteHeadTag(BinaryWriter binaryWriter, string tagSource)
    {
        WriteString(binaryWriter, MakeHeadTag(tagSource));
    }

    public static void WriteTailTag(BinaryWriter binaryWriter, string tagSource)
    {
        WriteString(binaryWriter, MakeTailTag(tagSource));
    }

    public static void WriteInteger(BinaryWriter binaryWriter, int value)
    {
        binaryWriter.Write(value);
    }

    public static void WriteInteger(BinaryWriter binaryWriter, string tagSource, int value)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, value);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteFloat(BinaryWriter binaryWriter, float value)
    {
        binaryWriter.Write(value);
    }

    public static void WriteFloat(BinaryWriter binaryWriter, string tagSource, float value)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteFloat(binaryWriter, value);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteIntegers(BinaryWriter binaryWriter, string tagSource, int[] integers)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", integers.Length);
        if (integers.Length > 0)
        {
            foreach (int i in integers)
            {
                WriteInteger(binaryWriter, i);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteU16(BinaryWriter binaryWriter, ushort value)
    {
        binaryWriter.Write(value);
    }

    public static void WriteU16(BinaryWriter binaryWriter, string tagSource, ushort value)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteU16(binaryWriter, value);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteU16s(BinaryWriter binaryWriter, string tagSource, ushort[] u16s)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", u16s.Length);
        if (u16s.Length > 0)
        {
            foreach (ushort i in u16s)
            {
                WriteU16(binaryWriter, i);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteIntegerAsU16s(BinaryWriter binaryWriter, string tagSource, int[] integers)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", integers.Length);
        if (integers.Length > 0)
        {
            foreach (int i in integers)
            {
                WriteU16(binaryWriter, (ushort)i);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteMatrix(BinaryWriter binaryWriter, string tagSource, Matrix4x4 matrix)
    {
        WriteHeadTag(binaryWriter, tagSource);
        binaryWriter.Write(matrix.m00);
        binaryWriter.Write(matrix.m10);
        binaryWriter.Write(matrix.m20);
        binaryWriter.Write(matrix.m30);
        binaryWriter.Write(matrix.m01);
        binaryWriter.Write(matrix.m11);
        binaryWriter.Write(matrix.m21);
        binaryWriter.Write(matrix.m31);
        binaryWriter.Write(matrix.m02);
        binaryWriter.Write(matrix.m12);
        binaryWriter.Write(matrix.m22);
        binaryWriter.Write(matrix.m32);
        binaryWriter.Write(matrix.m03);
        binaryWriter.Write(matrix.m13);
        binaryWriter.Write(matrix.m23);
        binaryWriter.Write(matrix.m33);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteLocalMatrix(BinaryWriter binaryWriter, string tagSource, Transform xform)
    {
        Matrix4x4 matrix = Matrix4x4.identity;
        matrix.SetTRS(xform.localPosition, xform.localRotation, xform.localScale);
        WriteMatrix(binaryWriter, tagSource, matrix);
    }

    public static void GetRelativeTRS(
        Transform target, Transform reference,
        out Vector3 relativePosition,
        out Quaternion relativeRotation,
        out Vector3 relativeScale)
    {
        // 두 Transform의 월드 행렬
        Matrix4x4 targetMatrix = target.localToWorldMatrix;
        Matrix4x4 referenceMatrix = reference.localToWorldMatrix;

        // B의 역행렬 * A = A를 B 기준으로 변환
        Matrix4x4 relativeMatrix = referenceMatrix.inverse * targetMatrix;

        // 행렬을 position, rotation, scale로 분해
        relativePosition = relativeMatrix.GetColumn(3);

        // 스케일 추출 (열 벡터 길이)
        relativeScale = new Vector3(
            relativeMatrix.GetColumn(0).magnitude,
            relativeMatrix.GetColumn(1).magnitude,
            relativeMatrix.GetColumn(2).magnitude
        );

        // 정규화 후 회전 추출
        Quaternion rot = Quaternion.LookRotation(
            relativeMatrix.GetColumn(2).normalized,
            relativeMatrix.GetColumn(1).normalized
        );
        relativeRotation = rot;
    }

    public static void WriteLocalTRS(BinaryWriter binaryWriter, string tagSource, Transform target)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteVector(binaryWriter, "Position", target.localPosition);
        WriteQuaternion(binaryWriter, "Rotation", target.localRotation);
        WriteVector(binaryWriter, "Scale", target.localScale);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteRelativeTRS( BinaryWriter binaryWriter, string tagSource, 
        Transform reference, Transform target
    )
    {
        Vector3 T = target.localPosition;
        Quaternion R = target.localRotation;
        Vector3 S = target.localScale;
        GetRelativeTRS(target, reference, out T, out R, out S);

        WriteHeadTag(binaryWriter, tagSource);
        WriteVector(binaryWriter, "Position", T);
        WriteQuaternion(binaryWriter, "Rotation", R);
        WriteVector(binaryWriter, "Scale", S);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteDressMatrix(BinaryWriter binaryWriter, string tagSource, Transform dressXform, Transform xform)
    {
        WriteMatrix(binaryWriter, tagSource, dressXform.worldToLocalMatrix * xform.localToWorldMatrix);
    }

    public static void WriteVector(BinaryWriter binaryWriter, Vector2 vec)
    {
        binaryWriter.Write(vec.x);
        binaryWriter.Write(vec.y);
    }

    public static void WriteVector(BinaryWriter binaryWriter, string tagSource, Vector2 vec)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteVector(binaryWriter, vec);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteVectors(BinaryWriter binaryWriter, string tagSource, Vector2[] vectors)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", vectors.Length);
        if (vectors.Length > 0)
        {
            foreach (Vector2 v in vectors)
            {
                WriteVector(binaryWriter, v);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteVector(BinaryWriter binaryWriter, Vector3 vec)
    {
        binaryWriter.Write(vec.x);
        binaryWriter.Write(vec.y);
        binaryWriter.Write(vec.z);
    }

    public static void WriteVector(BinaryWriter binaryWriter, string tagSource, Vector3 vec)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteVector(binaryWriter, vec);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteVectors(BinaryWriter binaryWriter, string tagSource, Vector3[] vectors)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", vectors.Length);
        if (vectors.Length > 0)
        {
            foreach (Vector3 v in vectors)
            {
                WriteVector(binaryWriter, v);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteBoneIndices(BinaryWriter binaryWriter, string tagSource, BoneWeight[] boneWeights)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", boneWeights.Length);
        if (boneWeights.Length > 0)
        {
            foreach (BoneWeight w in boneWeights)
            {
                WriteInteger(binaryWriter, w.boneIndex0);
                WriteInteger(binaryWriter, w.boneIndex1);
                WriteInteger(binaryWriter, w.boneIndex2);
                WriteInteger(binaryWriter, w.boneIndex3);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteBoneIndices(BinaryWriter binaryWriter, string tagSource, BoneWeight[] boneWeights, int[] boneIdxMap)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", boneWeights.Length);
        if (boneWeights.Length > 0)
        {
            foreach (BoneWeight w in boneWeights)
            {
                WriteInteger(binaryWriter, boneIdxMap[w.boneIndex0]);
                WriteInteger(binaryWriter, boneIdxMap[w.boneIndex1]);
                WriteInteger(binaryWriter, boneIdxMap[w.boneIndex2]);
                WriteInteger(binaryWriter, boneIdxMap[w.boneIndex3]);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteBoneWeights(BinaryWriter binaryWriter, string tagSource, BoneWeight[] boneWeights)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", boneWeights.Length);
        if (boneWeights.Length > 0)
        {
            foreach (BoneWeight w in boneWeights)
            {
                WriteFloat(binaryWriter, w.weight0);
                WriteFloat(binaryWriter, w.weight1);
                WriteFloat(binaryWriter, w.weight2);
                WriteFloat(binaryWriter, w.weight3);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteQuaternion(BinaryWriter binaryWriter, Quaternion quat)
    {
        binaryWriter.Write(quat.x);
        binaryWriter.Write(quat.y);
        binaryWriter.Write(quat.z);
        binaryWriter.Write(quat.w);
    }

    public static void WriteQuaternion(BinaryWriter binaryWriter, string tagSource, Quaternion quat)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteQuaternion(binaryWriter, quat);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteColor(BinaryWriter binaryWriter, Color c)
    {
        binaryWriter.Write(c.r);
        binaryWriter.Write(c.g);
        binaryWriter.Write(c.b);
        binaryWriter.Write(c.a);
    }

    public static void WriteColor(BinaryWriter binaryWriter, string tagSource, Color c)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteColor(binaryWriter, c);
        WriteTailTag(binaryWriter, tagSource);
    }

    public static void WriteColors(BinaryWriter binaryWriter, string tagSource, Color[] colors)
    {
        WriteHeadTag(binaryWriter, tagSource);
        WriteInteger(binaryWriter, "Cnt", colors.Length);
        if (colors.Length > 0)
        {
            foreach (Color c in colors)
            {
                WriteColor(binaryWriter, c);
            }
        }
        WriteTailTag(binaryWriter, tagSource);
    }
}
