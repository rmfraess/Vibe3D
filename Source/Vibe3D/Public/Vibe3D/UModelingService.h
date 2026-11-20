// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "UModelingService.generated.h"

class UDynamicMesh;

/**
 * Outcome of a modeling operation. Handle echoes the mesh the call acted on (or the one it
 * created), TriangleCount / VertexCount describe that mesh afterwards, AssetPath is set by the
 * asset-writing calls.
 */
USTRUCT(BlueprintType)
struct FModelingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 Handle = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 VertexCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString AssetPath;
};

/** Diagnostic snapshot of a session mesh — read it between stages instead of trusting a green result. */
USTRUCT(BlueprintType)
struct FModelingMeshInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 Handle = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 VertexCount = 0;

	/** Watertight (no open border edges). Booleans and voxel ops want this true. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bIsClosed = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 OpenBorderEdges = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 ConnectedComponents = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector BoundsMin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector BoundsMax = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float SurfaceArea = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float Volume = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 NumUVLayers = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bHasVertexColors = false;

	/** Distinct material IDs in use (empty when the mesh has no material attribute). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	TArray<int32> MaterialIds;

	/** Named selections currently stored on this handle. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	TArray<FString> Selections;
};

USTRUCT(BlueprintType)
struct FModelingSplitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	/** One new session handle per connected component, largest first. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	TArray<int32> Handles;
};

/** A point found on a mesh surface by RayCast / NearestPoint. */
USTRUCT(BlueprintType)
struct FModelingSurfacePoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bFound = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 TriangleID = -1;

	/** Ray parameter (distance along the ray) for RayCast; distance from the query point for NearestPoint. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float Distance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector BaryCoords = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;
};

/** Surface-to-surface distances between two session meshes (MeasureDistance). */
USTRUCT(BlueprintType)
struct FModelingDistanceReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float MaxDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float MinDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float AverageDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float RootMeanSquareDeviation = 0.f;
};

USTRUCT(BlueprintType)
struct FModelingBakeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	/** Saved Texture2D asset paths, in the order of the requested bake types. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	TArray<FString> TexturePaths;
};

/** Health of a UV layer (GetUVStats): what an unwrap looks like before you bake or paint on it. */
USTRUCT(BlueprintType)
struct FModelingUVStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	/** Connected UV islands (shells). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 NumIslands = 0;

	/** Triangles with no UVs assigned in this layer. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 NumUnsetTriangles = 0;

	/** Fraction of the 0-1 square covered by at least one island. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float Coverage = 0.f;

	/** Fraction of covered texels claimed by two or more islands (0 = no overlaps). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float OverlapFraction = 0.f;

	/** Texels per centimetre at a 1024 texture (multiply by resolution/1024). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float TexelsPerCmAt1K = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float MeshArea = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float UVArea = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector2D UVBoundsMin = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector2D UVBoundsMax = FVector2D::ZeroVector;
};

/** One bone for CreateBones. Transform is in mesh space (the space the vertices are in); an empty ParentName makes it the root. */
USTRUCT(BlueprintType)
struct FModelingBoneDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString ParentName;

	/** Bone frame in mesh space. Point its X axis along a hinge line and the control surface deflects by rolling the bone. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FTransform Transform;
};

/** A bone on a session mesh: hierarchy, reference pose, and how many vertices it influences. */
USTRUCT(BlueprintType)
struct FModelingBoneInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 Index = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString ParentName;

	/** Reference pose relative to the parent bone. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FTransform LocalTransform;

	/** Reference pose in mesh space. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FTransform MeshTransform;

	/** Vertices carrying a non-zero weight on this bone. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 InfluencedVertices = 0;
};

/** How far one mesh's vertices break a clearance rule against another (MeasurePenetration). */
USTRUCT(BlueprintType)
struct FModelingPenetrationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	/** Vertices close enough to the other surface (within MaxDistance, projecting onto it) to be judged. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 VerticesChecked = 0;

	/** Judged vertices on the wrong side, or closer than the margin. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 Violations = 0;

	/** Worst violation in cm (how far that vertex must move to satisfy the rule, margin included). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float MaxDepth = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	float AverageDepth = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector WorstPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 WorstVertex = -1;
};

/** What IntakeGeneratedMesh did to a generated model: read it before placing the prop. */
USTRUCT(BlueprintType)
struct FModelingIntakeReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bSuccess = false;

	/** One-paragraph summary (also the failure reason). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FString AssetPath;

	/** True when the asset already matched this file and these settings, so nothing was re-imported. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	bool bUpToDate = false;

	/** Static meshes the file produced (merged into one asset). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 SourceMeshCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 SourceTriangles = 0;

	/** LOD0 triangles after decimation. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 VertexCount = 0;

	/** Imported size before the fit, in cm (after the yaw). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector SourceSize = FVector::ZeroVector;

	/** Scale applied per axis; unequal values mean the fit stretched the model (FitAxis "Box"). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector ScaleApplied = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector BoundsMin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	FVector BoundsMax = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 LODCount = 1;

	/** Simple collision shapes on the asset (0 with Collision "None"). */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	int32 CollisionShapes = 0;

	/** Material slot assets, in slot order. */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	TArray<FString> MaterialPaths;

	/** One line per texture: "<path> <source W>x<H> max <cap>". */
	UPROPERTY(BlueprintReadWrite, Category = "Vibe3D|Modeling")
	TArray<FString> Textures;
};

/**
 * What Vibe3D adds on top of native GeometryScript in UE 5.9. Model with unreal.GeometryScript_*
 * directly (in execute_python); use these where the engine has a gap: a session of integer handles
 * wrapping UDynamicMesh (GetDynamicMesh hands it to GeometryScript), primitives that honour a
 * MaterialID (5.9's AppendTorus / AppendRevolvePolygon ignore PrimitiveOptions.MaterialID),
 * skeletal-mesh save with skeleton creation, material-slot assignment, from-scratch rigging and
 * selection-scoped skin-weight edits, UV coverage/overlap stats, procedural texture painting, and
 * polyline stamping/grooves. Named selections: build any GeometryScriptMeshSelection natively and
 * store it with SetSelection; tools taking a SelectionName use it ("" = whole mesh).
 *
 * Python: unreal.ModelingService.<snake_case>(...). See the Vibe3D `modeling` skill for the workflow.
 */
UCLASS(BlueprintType)
class VIBE3D_API UModelingService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =====================================================================
	// Session
	// =====================================================================

	/** Create an empty session mesh. Result.Handle is the new handle. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Create Mesh"))
	static FModelingResult CreateMesh();


	/**
	 * Load a SkeletalMesh asset's LOD (with bones and bone weights) into a new session mesh. Reads the LOD's source model,
	 * the editable vertex layout that SaveMeshToSkeletalMeshLOD writes back (a LOD generated by reduction only has render
	 * data; the message says so). The handle remembers the asset and LOD it came from. A LODIndex the asset does not have
	 * fails (before the post-jam update the engine silently read the nearest LOD instead and the result claimed the one asked for).
	 */
	// A tool again on this fork: native loads do not record the origin save_mesh_to_skeletal_mesh_lod needs for vertex order.
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Load Mesh From Skeletal Mesh"))
	static FModelingResult LoadMeshFromSkeletalMesh(const FString& AssetPath, int32 LODIndex = 0);

	/**
	 * Write a session mesh into ONE LOD of an existing SkeletalMesh, in place: skeleton, material slots, the other LODs,
	 * this LOD's normal/tangent settings and its section-to-slot mapping are kept. AssetPath "" and LODIndex -1 mean "where
	 * the handle was loaded from" (load_mesh_from_skeletal_mesh), so a round trip is load -> edit -> save_mesh_to_skeletal_mesh_lod(h).
	 * LODIndex == the asset's LOD count appends a LOD. Validated first, nothing written on failure: skin weights on every
	 * vertex, bones present in the skeleton (matched by name), bone and material indices in range, not a posed handle; the
	 * LOD is read back afterwards and its vertex count compared. A LOD whose build settings recompute normals keeps doing
	 * so (the handle's normals are then not used; the message says so). LODs generated from this one are regenerated from
	 * the new geometry by the engine.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Save Mesh To Skeletal Mesh LOD"))
	static FModelingResult SaveMeshToSkeletalMeshLOD(int32 Handle, const FString& AssetPath = TEXT(""), int32 LODIndex = -1, bool bSaveAsset = true);

	/** Skeleton, bone count, material slots and per-LOD source (source model or generated), vertex/triangle counts, bones used at runtime, removed bones. Read it before a LOD round trip. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Inspect Skeletal Mesh"))
	static FModelingResult InspectSkeletalMesh(const FString& AssetPath);


	/**
	 * Read a skinned component (body, garment, accessory; leader-pose followers included) AS IT IS POSED right now, with
	 * its asset's bones and weights: the source-model vertices of that LOD, skinned by the component's current bone
	 * transforms. Looks in the running PIE world first when bPreferPlayWorld (the runtime pose, post-process and corrective
	 * bones included), else the editor world. ComponentName "" takes the first skinned component (the message lists them
	 * all). bWorldSpace false = component space. Vertex IDs match the asset LOD, and the handle keeps the pose, so after
	 * fitting edits UnposeMesh brings the result back to the reference pose for SaveMeshToSkeletalMeshLOD.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Load Mesh From Skinned Component"))
	static FModelingResult LoadMeshFromSkinnedComponent(const FString& ActorLabel, const FString& ComponentName = TEXT(""), bool bWorldSpace = false,
		int32 LODIndex = 0, bool bPreferPlayWorld = true);

	/** Undo the pose of a handle from LoadMeshFromSkinnedComponent: every vertex goes back through the inverse of its blended bone transform (with the handle's current weights), so edits made in the pose land in the reference pose. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Unpose Mesh"))
	static FModelingResult UnposeMesh(int32 Handle);


	/** Free one session mesh (and its selections). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Release Mesh"))
	static bool ReleaseMesh(int32 Handle);

	/** Free every session mesh. Returns how many were released. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Release All Meshes"))
	static int32 ReleaseAllMeshes();

	/** Handles currently alive in the session. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "List Meshes"))
	static TArray<int32> ListMeshes();

	/** The UDynamicMesh behind a handle, for direct use with unreal.GeometryScript_* libraries (Python only).
	 *  AIIgnore keeps it off the MCP tool list: the registry treats a static UFunction without AICallable as an error. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AIIgnore, DisplayName = "Get Dynamic Mesh"))
	static UDynamicMesh* GetDynamicMesh(int32 Handle);

	/** Store a natively built GeometryScriptMeshSelection on a handle under SelectionName, for the tools below that take one. Returns the selected count, or -1. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Selection"))
	static int32 SetSelection(int32 Handle, const FString& SelectionName, const FGeometryScriptMeshSelection& Selection);

	/** Drop every named selection stored on a handle. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Clear Selections"))
	static bool ClearSelections(int32 Handle);
	/**
	 * Every vertex position as a plain array indexed by vertex ID (no GeometryScriptVectorList to convert). The array is
	 * MaxVertexID long: a mesh with ID gaps (after deletes or booleans) has (0,0,0) in the unused slots, so its length can
	 * exceed get_mesh_info().vertex_count. Edit the list and hand it back to SetVertexPositions. Empty for an unknown handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Get Vertex Positions"))
	static TArray<FVector> GetVertexPositions(int32 Handle);

	/**
	 * Move every vertex at once: Positions[i] becomes vertex i (same layout GetVertexPositions returned; gap slots are ignored).
	 * Topology, UVs, weights and selections are untouched; normals are not recomputed (call RecomputeNormals if the shape moved a lot).
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Vertex Positions"))
	static FModelingResult SetVertexPositions(int32 Handle, const TArray<FVector>& Positions);

	// =====================================================================
	// Primitives that honour MaterialID (5.9's generators ignore PrimitiveOptions.MaterialID for these)
	// =====================================================================

	/** Torus around the Z axis. MaterialID is stamped on the new triangles. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Append Torus"))
	static FModelingResult AppendTorus(int32 Handle, FTransform Transform, float MajorRadius = 50.f, float MinorRadius = 25.f, int32 MajorSteps = 16,
		int32 MinorSteps = 8, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	/** Revolve a 2D profile (X = radius offset, Y = height) around the Z axis: lathe shapes, rings, vases. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Append Revolve Polygon"))
	static FModelingResult AppendRevolvePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, float Radius = 100.f,
		int32 Steps = 16, float RevolveDegrees = 360.f, int32 MaterialID = 0);

	// =====================================================================
	// Assets
	// =====================================================================

	/**
	 * Write the mesh (with its bone weights) to a SkeletalMesh asset. SkeletonPath "" creates a new Skeleton asset at
	 * <AssetPath>_Skeleton from the bones on the mesh (CreateBones); an existing skeleton is bound as-is. One material slot
	 * is made per material ID — assign materials afterwards with SetAssetMaterials.
	 * When the asset already exists (and bReplaceExisting), LOD0 is replaced in place through the same validated writer as
	 * SaveMeshToSkeletalMeshLOD: skeleton, material slots, which section uses which slot and the other LODs are kept, bones
	 * are matched by name, and the handle's normals and tangents are stored as they are. Refused, with nothing written
	 * (these used to write a broken asset): vertices without skin weight, bones the asset's skeleton lacks, a material ID
	 * with no slot, a posed handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Save Mesh To Skeletal Mesh"))
	static FModelingResult SaveMeshToSkeletalMesh(int32 Handle, const FString& AssetPath, const FString& SkeletonPath = TEXT(""), bool bReplaceExisting = true, bool bSaveAsset = true);

	/** Assign material slots on a StaticMesh or SkeletalMesh asset: comma-separated material asset paths, in slot order; missing slots are added. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Asset Materials"))
	static FModelingResult SetAssetMaterials(const FString& AssetPath, const FString& MaterialPaths, bool bSaveAsset = true);


	// Not tools on this fork (UE 5.9 GeometryScript covers them); kept as plain C++ for intake's internal calls.
	static FModelingResult RotateMesh(int32 Handle, FRotator Rotation, FVector Origin = FVector::ZeroVector);
	static FModelingResult ScaleMesh(int32 Handle, FVector Scale, FVector Origin = FVector::ZeroVector);
	static FModelingResult SimplifyToTriangleCount(int32 Handle, int32 TriangleCount, bool bAllowSeamCollapse = true);
	static FModelingResult TranslateMesh(int32 Handle, FVector Translation);

	/** Method: "MinVolumeShapes", "ConvexHulls", "AlignedBoxes", "OrientedBoxes", "MinimalSpheres", "Capsules", "SweptHulls". Acts on the asset. */
	// Not a tool on this fork (UE 5.9 GeometryScript covers it); kept for internal callers.
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Generate Collision"))
	static FModelingResult GenerateCollision(const FString& AssetPath, const FString& Method = TEXT("MinVolumeShapes"), int32 MaxConvexHulls = 1,
		int32 ConvexHullTargetFaceCount = 25, bool bSaveAsset = true);

	/** Rebuild the asset's LOD chain: one entry per LOD as a fraction of LOD0 triangles (LOD0 first, e.g. [1.0, 0.5, 0.25]). */
	// Not a tool on this fork (UE 5.9 GeometryScript covers it); kept for internal callers.
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set LODs", ScriptName = "set_lods;set_lo_ds"))
	static FModelingResult SetLODs(const FString& AssetPath, const TArray<float>& PercentTrianglesPerLOD, bool bAutoComputeScreenSize = true, bool bSaveAsset = true);

	/**
	 * Generated-prop intake in one call: import a model file from an image-to-3D service (GLB/glTF, also FBX/OBJ), merge its
	 * meshes into ONE StaticMesh at AssetPath, turn it by YawDeg about Z, scale it to real-world size, move the pivot, decimate
	 * to MaxTriangles, cap its textures, build LODs and collision, and report what it did. Materials and textures land next to
	 * the mesh as MI_<Name>_<i> / T_<Name>_<Role>; nothing else from the import is kept.
	 * FitAxis: "Height" (Z extent = TargetSizeCm), "Width" (X), "Depth" (Y), "Longest", "Box" (stretch to TargetBoxCm, per axis),
	 * "None". Pivot: "BottomCenter", "Center", "TopCenter" (hanging lamps), "None". Collision: "ConvexHulls" (MaxConvexHulls
	 * pieces), "Box", "None", or any GenerateCollision method. LODPercents: comma list of LOD0 fractions ("1,0.5,0.25"; "" = one
	 * LOD). Re-running with the same file and settings is a no-op (bUpToDate) unless bForce; a changed file or setting replaces
	 * the asset in place, so placed actors keep working. A StaticMesh at AssetPath that intake did not make is only replaced
	 * with bForce. MaxTextureSize sets each texture's "Maximum Texture Size" (what is built, cooked and loaded); the source
	 * pixels in the .uasset stay at full resolution.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Intake Generated Mesh"))
	static FModelingIntakeReport IntakeGeneratedMesh(const FString& SourceFile, const FString& AssetPath, float TargetSizeCm = 100.f,
		const FString& FitAxis = TEXT("Height"), float YawDeg = 0.f, const FString& Pivot = TEXT("BottomCenter"), int32 MaxTriangles = 20000,
		int32 MaxTextureSize = 1024, const FString& Collision = TEXT("ConvexHulls"), int32 MaxConvexHulls = 4, const FString& LODPercents = TEXT(""),
		FVector TargetBoxCm = FVector::ZeroVector, bool bEnableNanite = false, bool bForce = false, bool bSaveAsset = true);


	// =====================================================================
	// Rigging: bones and skin weights authored on the mesh itself
	// =====================================================================

	/** Replace the mesh's bone hierarchy. The first bone is the root; every other bone names a parent listed before it. Every vertex starts bound to the root. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Create Bones"))
	static FModelingResult CreateBones(int32 Handle, const TArray<FModelingBoneDef>& Bones);

	/** Bind the vertices of a selection ("" = all) to a bone. Weight 1 replaces their weights; a lower weight blends with what is there. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Bind Selection To Bone"))
	static FModelingResult BindSelectionToBone(int32 Handle, const FString& SelectionName, const FString& BoneName, float Weight = 1.f);


	// =====================================================================
	// More simplification
	// =====================================================================



	// =====================================================================
	// Spatial queries and sampling
	// =====================================================================







	// =====================================================================
	// Hulls and comparison
	// =====================================================================




	// =====================================================================
	// Fit one mesh against another (garments on bodies, layers of clothing). Both handles must be in the same space.
	// A vertex is judged only when it lies within MaxDistance of the other surface AND projects onto it (the offset to the
	// nearest point runs along that face's normal), so a head is never dragged into a collar. The other mesh's face
	// normals say which side is "outside".
	// =====================================================================

	/** Report how far Handle's vertices break the rule "stay on Side of OtherHandle by Margin cm". Side "Inside" (skin under a garment) or "Outside" (a garment over skin). OutSelectionName (optional) stores the offending vertices. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Measure Penetration"))
	static FModelingPenetrationReport MeasurePenetration(int32 Handle, int32 OtherHandle, const FString& Side = TEXT("Inside"), float Margin = 0.f, float MaxDistance = 5.f,
		const FString& SelectionName = TEXT(""), const FString& OutSelectionName = TEXT(""));

	/** Move Handle's vertices that poke out through OtherHandle (or sit closer than Margin under it) inward along the other surface's normal until Margin under it — skin pushed under clothes. SmoothIterations feathers the move into that many rings of neighbours so no crease shows. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Push Inside"))
	static FModelingResult PushInside(int32 Handle, int32 OtherHandle, float Margin = 0.3f, const FString& SelectionName = TEXT(""), float MaxDistance = 5.f, int32 SmoothIterations = 2);

	/** Move Handle's vertices that sink into OtherHandle (or sit closer than Margin above it) outward along the other surface's normal until Margin over it — a garment pushed off the body or off the face. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Push Outside"))
	static FModelingResult PushOutside(int32 Handle, int32 OtherHandle, float Margin = 0.3f, const FString& SelectionName = TEXT(""), float MaxDistance = 5.f, int32 SmoothIterations = 2);

	/** Select Handle's triangles hidden under OtherHandle: every vertex (or any, with bAllVertices false) at least Margin inside it and covered by it. delete_faces on the result drops skin under clothes or an inner garment under an outer one. Returns the triangle count. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Select Hidden By"))
	static int32 SelectHiddenBy(int32 Handle, const FString& SelectionName, int32 OtherHandle, float Margin = 0.5f, float MaxDistance = 5.f, bool bAllVertices = true);


	// =====================================================================
	// More skeletal
	// =====================================================================



	/** Select every vertex where the listed bones (comma-separated) together hold at least MinWeight of the skinning — the region a bone actually drags. Returns the vertex count. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Select By Bone Weight"))
	static int32 SelectByBoneWeight(int32 Handle, const FString& SelectionName, const FString& BoneNames, float MinWeight = 0.25f);

	/**
	 * Scale the listed bones' influence on a selection ("" = whole mesh): Scale 0 removes it, 0.25 keeps a quarter. The freed
	 * weight goes to ReceiverBoneName when given, otherwise to each vertex's strongest other influence. The targeted fix for
	 * clothing that follows the wrong bone — a collar lifting with the head, a skirt hem glued to a thigh. Vertices bound
	 * only to the listed bones are skipped unless a receiver is named.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Scale Bone Weights"))
	static FModelingResult ScaleBoneWeights(int32 Handle, const FString& SelectionName, const FString& BoneNames, float Scale = 0.f, const FString& ReceiverBoneName = TEXT(""));

	/** Laplacian-smooth skin weights across a selection ("" = whole mesh) — soften the seam after BindSelectionToBone or ScaleBoneWeights so a rigid patch blends into its surroundings. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Smooth Selection Bone Weights"))
	static FModelingResult SmoothSelectionBoneWeights(int32 Handle, const FString& SelectionName, int32 Iterations = 3, float Alpha = 0.5f);

	/**
	 * Skin a mesh (typically a static accessory: a tie, dog tags, a holster) to the surface it sits on, so it follows that
	 * surface instead of riding rigidly on one bone. SourceHandle is a skinned session mesh in the SAME space as Handle
	 * (load_mesh_from_skeletal_mesh for the reference pose, load_mesh_from_skinned_component for the pose as displayed).
	 * Mode: "Surface" (weights of the closest surface point), "Inpaint" (closest point where the match is clear, smoothly
	 * filled elsewhere: better for pieces that stand off the surface), "Rigid" (every vertex 100% on RigidBoneName, or on
	 * the bone the surface under it is most weighted to). The result carries the source's bones, or those of
	 * BonesFromSkeletalMesh (e.g. the body the accessory will follow as a leader-pose follower; weights are matched by name).
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Skin To Surface"))
	static FModelingResult SkinToSurface(int32 Handle, int32 SourceHandle, const FString& Mode = TEXT("Surface"), const FString& RigidBoneName = TEXT(""),
		const FString& BonesFromSkeletalMesh = TEXT(""), int32 SmoothIterations = 0, float SmoothStrength = 0.f);

	/**
	 * Fix a skinned follower (accessory, garment) that streaks at lower LODs: for each of its LODs, every bone the reference
	 * mesh (the leader, usually the body) does not keep at that LOD hands its weight to its nearest kept ancestor, then the
	 * LOD is written back in place. The follower's last LOD also covers the reference's lower LODs. Reports, per LOD, which
	 * bones were remapped and how many vertices moved. A changed LOD that was generated by reduction becomes a custom LOD.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Remap LOD Bones"))
	static FModelingResult RemapLODBones(const FString& AssetPath, const FString& ReferenceSkeletalMeshPath, bool bSaveAsset = true);

	/** Per-bone skin weight report over a selection ("" = whole mesh): influenced vertices, average/max weight per bone, weight-sum range, max influences. Verify a repair before saving. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Inspect Bone Weights"))
	static FModelingResult InspectBoneWeights(int32 Handle, const FString& SelectionName = TEXT(""));

	// =====================================================================
	// UVs and textures
	// =====================================================================

	/** Islands, coverage, overlap and texel density of a UV layer — check an unwrap before baking or painting. GridResolution is the raster used for coverage/overlap. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Get UV Stats", ScriptName = "get_uv_stats"))
	static FModelingUVStats GetUVStats(int32 Handle, int32 UVLayer = 0, int32 GridResolution = 512);


	// =====================================================================
	// More bakes and image tools
	// =====================================================================


	/** Tileable fractal (Perlin fBm) grayscale texture — grunge, wear masks, height detail. Scale is the number of noise cells across the image. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Create Noise Texture"))
	static FModelingResult CreateNoiseTexture(const FString& AssetPath, int32 Width = 1024, int32 Height = 1024, float Scale = 8.f, int32 Octaves = 4, float Persistence = 0.5f,
		int32 Seed = 0, bool bSaveAsset = true);

	/** Draw into an existing 8-bit texture in UV space (0-1, V down). Shape: "Line" (polyline through Points), "Dots" (discs every SpacingPx along the polyline), "Rect" (Points[0]..Points[1] filled), "Fill" (whole texture). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Draw On Texture"))
	static FModelingResult DrawOnTexture(const FString& AssetPath, const FString& Shape, const TArray<FVector2D>& Points, FLinearColor Color, float ThicknessPx = 2.f,
		float SpacingPx = 8.f, bool bSaveAsset = true);

	// =====================================================================
	// Polylines
	// =====================================================================

	/** Stamp another session mesh every Spacing along a polyline, X axis along the line and Z along UpVector — rivet rows, fastener lines, light strips. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Append Mesh Along Polyline"))
	static FModelingResult AppendMeshAlongPolyline(int32 Handle, int32 OtherHandle, const TArray<FVector>& Points, float Spacing = 10.f, FVector UpVector = FVector::UpVector, float Scale = 1.f);

	/** Cut a rectangular groove (Width across, Depth into the surface along -UpVector, and as far out along +UpVector) following a polyline — recessed panel lines and seams. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Cut Groove Along Polyline"))
	static FModelingResult CutGrooveAlongPolyline(int32 Handle, const TArray<FVector>& Points, float Width = 1.f, float Depth = 1.f, FVector UpVector = FVector::UpVector);
public:
	// Restored from upstream: the native replacement does not do all of this (cut audit).
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Append Stairs"))
	static FModelingResult AppendStairs(int32 Handle, FTransform Transform, float StepWidth = 100.f, float StepHeight = 20.f, float StepDepth = 30.f,
		int32 NumSteps = 8, bool bFloating = false, int32 MaterialID = 0);

	/** Bake maps from SourceHandle onto TargetHandle's UVs. BakeTypes: comma list of "TangentNormal", "ObjectNormal", "AmbientOcclusion", "Curvature", "Position", "BentNormal". Resolution 16..8192 (power of two). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Bake Textures"))
	static FModelingBakeResult BakeTextures(int32 TargetHandle, int32 SourceHandle, const FString& BakeTypes, int32 Resolution, const FString& OutputFolder,
		const FString& BaseName, int32 TargetUVLayer = 0, int32 SamplesPerPixel = 1);

	/** Transfer textures from SourceHandle's UVs onto TargetHandle's: one path = a plain texture bake, several (comma-separated, in source material-ID order) = a multi-texture bake. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Bake Texture Transfer"))
	static FModelingBakeResult BakeTextureTransfer(int32 TargetHandle, int32 SourceHandle, const FString& SourceTexturePaths, int32 Resolution, const FString& OutputFolder,
		const FString& BaseName, int32 TargetUVLayer = 0, int32 SourceUVLayer = 0, int32 SamplesPerPixel = 1);

	/** Duplicate a session mesh into a new handle. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Copy Mesh"))
	static FModelingResult CopyMesh(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Delete Faces"))
	static FModelingResult DeleteFaces(int32 Handle, const FString& SelectionName);

	/** Displace along normals by a texture sampled through a UV layer (height maps). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Displace From Texture"))
	static FModelingResult DisplaceFromTexture(int32 Handle, const FString& SelectionName, const FString& TexturePath, float Magnitude = 10.f, int32 UVLayer = 0);

	/** Grow (or shrink with bContract) a selection by N rings of neighbours; stores it under NewSelectionName. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Expand Contract Selection"))
	static int32 ExpandContractSelection(int32 Handle, const FString& SelectionName, const FString& NewSelectionName, int32 Iterations = 1, bool bContract = false);

	/** Extrude selected faces. Zero Direction = along each face's average normal; otherwise a fixed world direction. Negative distance recesses. The selection then holds the moved faces. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Extrude Faces"))
	static FModelingResult ExtrudeFaces(int32 Handle, const FString& SelectionName, float Distance = 10.f, FVector Direction = FVector::ZeroVector);

	/** Counts, bounds, closedness, components, UV layers, material IDs, stored selections. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Get Mesh Info"))
	static FModelingMeshInfo GetMeshInfo(int32 Handle);

	/** Inset selected faces (panel lines, frames). Softness rounds the inset boundary. The selection then holds the inner faces, ready to extrude. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Inset Faces"))
	static FModelingResult InsetFaces(int32 Handle, const FString& SelectionName, float Distance = 5.f, float Softness = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Invert Selection"))
	static int32 InvertSelection(int32 Handle, const FString& SelectionName, const FString& NewSelectionName);

	/** LayoutType: "Repack" (pack islands into 0-1 for TextureResolution), "Stack" (all islands on top of each other), "Normalize" (scale to unit texel density), "Transform" (Scale + Translation). Works on a selection's islands. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Layout UV", ScriptName = "layout_uv"))
	static FModelingResult LayoutUV(int32 Handle, int32 UVLayer = 0, const FString& LayoutType = TEXT("Repack"), const FString& SelectionName = TEXT(""), int32 TextureResolution = 2048,
		float Scale = 1.f, FVector2D Translation = FVector2D::ZeroVector);

	/** Bones on the mesh with their reference poses and influenced-vertex counts — check a rig before saving it. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "List Bones"))
	static TArray<FModelingBoneInfo> ListBones(int32 Handle);

	/** Load the static mesh of a level actor (by label) into a new session mesh; bWorldSpace bakes the actor transform in. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Load Mesh From Actor"))
	static FModelingResult LoadMeshFromActor(const FString& ActorLabel, bool bWorldSpace = false, int32 LODIndex = 0);

	/** Perlin displacement along normals (rock, damage, organic wobble). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Noise"))
	static FModelingResult Noise(int32 Handle, const FString& SelectionName, float Magnitude = 5.f, float Frequency = 0.25f, int32 RandomSeed = 0);

	/** Offset selected faces along their normals (parallel face offset). The selection then holds the moved faces. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Offset Faces"))
	static FModelingResult OffsetFaces(int32 Handle, const FString& SelectionName, float Distance = 10.f);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Outset Faces"))
	static FModelingResult OutsetFaces(int32 Handle, const FString& SelectionName, float Distance = 5.f, float Softness = 0.f);

	/** Pack the islands of each material ID into their own full 0-1 range so every material slot gets its own texture set at full resolution. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Pack UV Per Material", ScriptName = "pack_uv_per_material"))
	static FModelingResult PackUVPerMaterial(int32 Handle, int32 UVLayer = 0, int32 TextureResolution = 2048);

	/** Method: "Planar", "Box", "Cylinder"; the transform positions/scales the projector. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Project UV"))
	static FModelingResult ProjectUV(int32 Handle, const FString& Method, FTransform ProjectorTransform, int32 UVLayer = 0, const FString& SelectionName = TEXT(""));

	/** Flatten each island with a conformal unwrap. IslandSource: "Polygroups" (one island per group — define them with SetPolygroup / ComputePolygroups) or "UVIslands" (re-flatten the existing islands). Method: "SpectralConformal" (default), "Conformal", "ExpMap". */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Recompute UVs", ScriptName = "recompute_uvs;recompute_u_vs"))
	static FModelingResult RecomputeUVs(int32 Handle, int32 UVLayer = 0, const FString& IslandSource = TEXT("Polygroups"), const FString& Method = TEXT("SpectralConformal"),
		const FString& SelectionName = TEXT(""), bool bAlignIslandsWithAxes = true);

	/**
	 * Repair degenerate triangles, drop tiny floating pieces, compact — the standard cleanup after
	 * imports and booleans. Collapsing degenerates can punch holes in a watertight mesh, so with
	 * bKeepClosed (default) a mesh that was closed on the way in is re-closed on the way out;
	 * pass false if you want the raw result.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Repair"))
	static FModelingResult Repair(int32 Handle, float MinComponentVolume = 0.0001f, int32 MinComponentTriangles = 1, bool bKeepClosed = true);

	/** Write the mesh to a StaticMesh asset (created, or LOD0 replaced when it exists and bReplaceExisting). Saves the package. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Save Mesh To Static Mesh"))
	static FModelingResult SaveMeshToStaticMesh(int32 Handle, const FString& AssetPath, bool bReplaceExisting = true, bool bEnableCollision = true,
		bool bEnableNanite = false, bool bSaveAsset = true);

	/** Select the whole connected piece of geometry nearest to Point — a control surface isolated by slot cuts, a bolt on a plate. Returns the triangle count. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Select Connected"))
	static int32 SelectConnected(int32 Handle, const FString& SelectionName, FVector Point);

	/** Bounding box of a selection (Min, Max). IsValid is false, with a zero box, when the selection is empty or unknown. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Selection Bounds"))
	static FBox SelectionBounds(int32 Handle, const FString& SelectionName);
	/** C++ form of SelectionBounds: false when the selection is empty. Not reflected: an output reference would become a
	 *  required input of the MCP tool and its value would be dropped. */
	static bool SelectionBounds(int32 Handle, const FString& SelectionName, FVector& OutMin, FVector& OutMax);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Read", DisplayName = "Selection Count"))
	static int32 SelectionCount(int32 Handle, const FString& SelectionName);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Material ID"))
	static FModelingResult SetMaterialID(int32 Handle, const FString& SelectionName, int32 MaterialID);

	/** Give every triangle of a selection one polygroup: GroupID >= 0 sets that id, -1 allocates a new group. Polygroups are the island source for RecomputeUVs("Polygroups") and PatchBuilder. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Polygroup"))
	static FModelingResult SetPolygroup(int32 Handle, const FString& SelectionName, int32 GroupID = -1);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Vertex Color"))
	static FModelingResult SetVertexColor(int32 Handle, const FString& SelectionName, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Smooth"))
	static FModelingResult Smooth(int32 Handle, const FString& SelectionName, int32 Iterations = 5, float Alpha = 0.2f);

	/** Split into connected components, each becoming a new session handle. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Split By Components"))
	static FModelingSplitResult SplitByComponents(int32 Handle);

	/** Move / scale / rotate the UVs of a selection ("" = all) in UV space: scale and rotation are about Origin (0.5,0.5 = texture centre). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Transform UV", ScriptName = "transform_uv"))
	static FModelingResult TransformUV(int32 Handle, int32 UVLayer, const FString& SelectionName, FVector2D Translation, FVector2D Scale, float RotationDeg = 0.f, FVector2D Origin = FVector2D(0.5, 0.5));

	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Translate Selection"))
	static FModelingResult TranslateSelection(int32 Handle, const FString& SelectionName, FVector Delta);

public:
	// Restored from upstream: the native replacement does not do all of this (cut audit).
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, AIAccessMode = "Write", DisplayName = "Set Num UV Layers"))
	static FModelingResult SetNumUVLayers(int32 Handle, int32 NumLayers);

};
