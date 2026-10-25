using NHSE.Core;

namespace NHSE.Mac;

// Pending port of NHSE.WinForms' SaveRoomFloorWallEditor: replaced by SaveRoomFloorWallEditor.axaml + SaveRoomFloorWallEditor.axaml.cs.
public sealed class SaveRoomFloorWallEditor(GSaveRoomFloorWall v) : PendingWindow
{
    public GSaveRoomFloorWall Entity { get; } = v;
}
