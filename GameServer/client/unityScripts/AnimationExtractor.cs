using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections.Generic;
using System.IO;

public class AnimationExtractorWindow : EditorWindow
{
    [System.Serializable]
    public class AnimInfo
    {
        public string name;
        public string skeletonEnumeration;
        public float translationThreshold = 0.001f;
        public float rotationThreshold = 0.001f;
        public float scaleThreshold = 0.001f;
        public float bakedSampleRate = 24.0f;
    }

    [System.Serializable]
    public class AnimEntry
    {
        public AnimationClip clip;
        public AnimInfo info = new AnimInfo();
    }

    [System.Serializable]
    public class Sample
    {
        public float time;
        public Vector3 translation;
        public Quaternion rotation;
        public Vector3 scale;
    }

    [SerializeField]
    private List<AnimEntry> animList = new List<AnimEntry>();

    private ReorderableList animListUI;

    private GameObject skeleton;
    private GameObject sampleTarget;
    private string targetName = "";

    private BinaryWriter binaryWriter = null;

    private List<Transform> bones = null;
    private List<Matrix4x4> bindposes = null;

    private Vector2 scrollPos;

    [MenuItem("Tools/Animation Extractor")]
    public static void OpenWindow()
    {
        GetWindow<AnimationExtractorWindow>("Animation Extractor");
    }

    private void OnEnable()
    {
        animListUI = new ReorderableList(animList, typeof(AnimEntry), true, true, true, true);

        // Header
        animListUI.drawHeaderCallback = (Rect rect) =>
        {
            EditorGUI.LabelField(rect, "Animation Clips");
        };

        // Element GUI
        animListUI.drawElementCallback = (Rect rect, int index, bool isActive, bool isFocused) =>
        {
            var entry = animList[index];
            float lineHeight = EditorGUIUtility.singleLineHeight;
            float y = rect.y + 2;

            // AnimationClip field
            entry.clip = (AnimationClip)EditorGUI.ObjectField(
                new Rect(rect.x, y, rect.width, lineHeight),
                entry.clip,
                typeof(AnimationClip),
                false
            );

            y += lineHeight + 2;

            if (entry.clip != null)
            {
                // Show AnimInfo fields
                entry.info.name = EditorGUI.TextField(
                    new Rect(rect.x + 10, y, rect.width - 10, lineHeight),
                    "Name", entry.info.name
                );

                y += lineHeight + 2;

                entry.info.skeletonEnumeration = EditorGUI.TextField(
                    new Rect(rect.x + 10, y, rect.width - 10, lineHeight),
                    "SkeletonEnumeration", entry.info.skeletonEnumeration
                );

                y += lineHeight + 2;

                entry.info.bakedSampleRate = EditorGUI.FloatField(
                    new Rect(rect.x + 10, y, rect.width - 10, lineHeight),
                    "Baked Sample Rate", entry.info.bakedSampleRate
                );

                y += lineHeight + 2;

                entry.info.translationThreshold = EditorGUI.FloatField(
                    new Rect(rect.x + 10, y, rect.width - 10, lineHeight),
                    "Translation Threshold",
                    entry.info.translationThreshold
                );

                y += lineHeight + 2;

                entry.info.rotationThreshold = EditorGUI.FloatField(
                    new Rect(rect.x + 10, y, rect.width - 10, lineHeight),
                    "Rotation Threshold",
                    entry.info.rotationThreshold
                );

                y += lineHeight + 2;

                entry.info.scaleThreshold = EditorGUI.FloatField(
                    new Rect(rect.x + 10, y, rect.width - 10, lineHeight),
                    "Scale Threshold",
                    entry.info.scaleThreshold
                );
            }
        };

        // Reserve extra height for AnimInfo
        animListUI.elementHeightCallback = (int index) =>
        {
            var entry = animList[index];
            if (entry.clip == null)
                return EditorGUIUtility.singleLineHeight + 6;

            // 1 clip field + 6 info fields
            return EditorGUIUtility.singleLineHeight * 7 + 12;
        };
    }

    private void OnGUI()
    {
        scrollPos = EditorGUILayout.BeginScrollView(scrollPos);

        EditorGUILayout.LabelField("🎨 Animation Extractor Tool", EditorStyles.boldLabel);
        EditorGUILayout.Space();

        skeleton = (GameObject)EditorGUILayout.ObjectField("Target Skeleton", skeleton, typeof(GameObject), true);
        EditorGUILayout.Space();

        sampleTarget = (GameObject)EditorGUILayout.ObjectField("Sample Target", sampleTarget, typeof(GameObject), true);
        EditorGUILayout.Space();

        targetName = EditorGUILayout.TextField("AnimationSet Name:", targetName);
        EditorGUILayout.Space();

        // Draw Animation List UI
        animListUI.DoLayoutList();

        EditorGUILayout.Space();

        if (GUILayout.Button("💾 Export as a binary(.bin)"))
        {
            ExportBinary();
        }

        EditorGUILayout.EndScrollView();
    }

    List<Matrix4x4> SampleMatrices(AnimationClip clip, Transform bone, Matrix4x4 bindpose, float fps)
    {
        var matrices = new List<Matrix4x4>();
        int sampleCnt = (int)(clip.length * clip.frameRate);

        for (int i = 0; i < sampleCnt; ++i)
        {
            float t = i / fps;

            clip.SampleAnimation(sampleTarget, t);
            matrices.Add(sampleTarget.transform.worldToLocalMatrix * bone.localToWorldMatrix * bindpose);
        }
        return matrices;
    }

    List<Sample> SampleClip(AnimationClip clip, Transform bone, float fps)
    {
        var samples = new List<Sample>();
        int sampleCnt = (int)(clip.length * clip.frameRate);

        for (int i = 0; i < sampleCnt; ++i)
        {
            float t = i / fps;

            clip.SampleAnimation(sampleTarget, t);

            samples.Add(new Sample
            {
                time = t,
                translation = bone.localPosition,
                rotation = bone.localRotation,
                scale = bone.localScale
            });
        }
        return samples;
    }

    List<Sample> SampleClip(AnimationClip clip, Transform bone)
    {
        return SampleClip(clip, bone, clip.frameRate);
    }

    bool ExceedsThreshold( Sample a, Sample b, Sample mid,
        float tTrans, float tRot, float tScale
    )
    {
        float alpha = (mid.time - a.time) / (b.time - a.time);

        // 보간된 예상값
        Vector3 interpPos = Vector3.Lerp(a.translation, b.translation, alpha);
        Quaternion interpRot = Quaternion.Slerp(a.rotation, b.rotation, alpha);
        Vector3 interpScale = Vector3.Lerp(a.scale, b.scale, alpha);

        // 실제 샘플 값과의 오차
        float transError = Vector3.Distance(interpPos, mid.translation);
        float rotError = Quaternion.Angle(interpRot, mid.rotation);
        float scaleError = Vector3.Distance(interpScale, mid.scale);

        return transError > tTrans ||
               rotError > tRot ||
               scaleError > tScale;
    }

    void SimplifyRecursive( List<Sample> input, int start, int end,
        float tTrans, float tRot, float tScale,
        List<bool> keep
    )
    {
        int maxIndex = -1;
        bool exceeded = false;

        for (int i = start + 1; i < end; i++)
        {
            if (ExceedsThreshold(input[start], input[end], input[i],
                                 tTrans, tRot, tScale))
            {
                exceeded = true;
                maxIndex = i;
                break;
            }
        }

        if (exceeded)
        {
            keep[maxIndex] = true;
            SimplifyRecursive(input, start, maxIndex, tTrans, tRot, tScale, keep);
            SimplifyRecursive(input, maxIndex, end, tTrans, tRot, tScale, keep);
        }
    }

    List<Sample> Simplify( List<Sample> input,
        float tTrans, float tRot, float tScale
    )
    {
        int n = input.Count;
        var keep = new List<bool>(new bool[n]);
        keep[0] = true;
        keep[n - 1] = true;

        SimplifyRecursive(input, 0, n - 1, tTrans, tRot, tScale, keep);

        var output = new List<Sample>();
        for (int i = 0; i < n; i++)
            if (keep[i]) output.Add(input[i]);

        return output;
    }

    void ExtractClip(AnimEntry anim)
    {
        ExtractUtil.WriteHeadTag(binaryWriter, "Clip");

        ExtractUtil.WriteText(binaryWriter, "Name", anim.info.name);
        ExtractUtil.WriteText(binaryWriter, "SkeletonEnumeration", anim.info.skeletonEnumeration);

        ExtractUtil.WriteFloat(binaryWriter, "Duration", anim.clip.length);
        ExtractUtil.WriteText(binaryWriter, "WrapMode", anim.clip.wrapMode.ToString());

        int idxBone = 0;

        // clip.SampleAnimation(skeleton, time)의 방법으로 샘플된 프레임들을
        // Threshold 값들을 바탕으로 키 프레임 계산해서 추출
        ExtractUtil.WriteHeadTag(binaryWriter, "KeyFrames");

        foreach (Transform bone in bones)
        {
            ExtractUtil.WriteInteger(binaryWriter, "Bone", idxBone);
            List<Sample> samples = SampleClip(anim.clip, bone);
            List<Sample> keyFrames = Simplify(samples,
                anim.info.translationThreshold,
                anim.info.rotationThreshold,
                anim.info.scaleThreshold
            );

            Debug.Log("Bone " + idxBone + ": SampleCnt(" + samples.Count + "), KeyFrameCnt: (" +  keyFrames.Count + ")");

            ExtractUtil.WriteInteger(binaryWriter, "KeyFrameCnt", keyFrames.Count);
            foreach (Sample sample in keyFrames)
            {
                ExtractUtil.WriteHeadTag(binaryWriter, "KeyFrame");
                ExtractUtil.WriteFloat(binaryWriter, "Time", sample.time);
                ExtractUtil.WriteVector(binaryWriter, "Translation", sample.translation);
                ExtractUtil.WriteQuaternion(binaryWriter, "Rotation", sample.rotation);
                ExtractUtil.WriteVector(binaryWriter, "Scale", sample.scale);
                ExtractUtil.WriteTailTag(binaryWriter, "KeyFrame");
            }
            ++idxBone;
        }

        ExtractUtil.WriteTailTag(binaryWriter, "KeyFrames");

        // 미리 정한 fps로 구운 샘플들 추출
        idxBone = 0;

        ExtractUtil.WriteHeadTag(binaryWriter, "BakedSamples");
        ExtractUtil.WriteFloat(binaryWriter, "SampleRate", anim.info.bakedSampleRate);

        foreach (Transform bone in bones)
        {
            ExtractUtil.WriteInteger(binaryWriter, "Bone", idxBone);
            List<Matrix4x4> matrices = SampleMatrices(anim.clip, bone, bindposes[idxBone], anim.info.bakedSampleRate);

            Debug.Log("Bone " + idxBone + ": BakedSampleCnt(" + matrices.Count + ")");

            ExtractUtil.WriteInteger(binaryWriter, "sampleCnt", matrices.Count);

            int i = 0;

            foreach (Matrix4x4 mtx in matrices)
            {
                ExtractUtil.WriteHeadTag(binaryWriter, "Sample");
                ExtractUtil.WriteMatrix(binaryWriter, "Matrix", mtx);
                ExtractUtil.WriteTailTag(binaryWriter, "Sample");

                ++i;
            }
            ++idxBone;
        }

        ExtractUtil.WriteTailTag(binaryWriter, "BakedSamples");

        ExtractUtil.WriteTailTag(binaryWriter, "Clip");
    }

    void ProcessBoneHierarchy(Transform bone)
    {
        bones.Add(bone);
        bindposes.Add(bone.worldToLocalMatrix * skeleton.transform.localToWorldMatrix);

        for (int i = 0; i < bone.childCount; ++i)
        {
            ProcessBoneHierarchy(bone.GetChild(i));
        }
    }
    
    void ProcessBones()
    {
        bones = new List<Transform>();
        bindposes = new List<Matrix4x4>();
        ProcessBoneHierarchy(skeleton.transform);
    }

    void ExportBinary()
    {
        string path = EditorUtility.SaveFilePanel("Export Binary", "ExportedAssets", "output.anim", "anim");
        binaryWriter = new BinaryWriter(File.Open(path, FileMode.Create));

        ProcessBones();

        ExtractUtil.WriteText(binaryWriter, "AnimationSetName", targetName);
        ExtractUtil.WriteInteger(binaryWriter, "ClipCnt", animList.Count);
        ExtractUtil.WriteInteger(binaryWriter, "BoneCnt", bones.Count);
        foreach (AnimEntry anim in animList)
        {
            ExtractClip(anim);
        }

        binaryWriter.Flush();
        binaryWriter.Close();
        Debug.Log("Animation Binary Write Completed");
    }
}