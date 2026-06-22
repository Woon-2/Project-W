using UnityEngine;

public enum SocketType
{
    None,
    RightHand,
    LeftHand
}

[System.Serializable]
public class SocketOffset
{
    public SocketType socketType;
    public Vector3 translation;
    public Vector3 rotation;  // Euler
    public Vector3 scale = Vector3.one;
}

public class BoneSocket : MonoBehaviour
{
    public SocketType socketType;
}
