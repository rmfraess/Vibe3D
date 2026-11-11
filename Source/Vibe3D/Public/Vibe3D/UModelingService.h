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
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Create Mesh"))
	static FModelingResult CreateMesh();

	/** Free one session mesh (and its selections). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Release Mesh"))
	static bool ReleaseMesh(int32 Handle);

	/** Free every session mesh. Returns how many were released. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Release All Meshes"))
	static int32 ReleaseAllMeshes();

	/** Handles currently alive in the session. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "List Meshes"))
	static TArray<int32> ListMeshes();

	/** The UDynamicMesh behind a handle, for direct use with unreal.GeometryScript_* libraries (Python only).
	 *  AIIgnore keeps it off the MCP tool list: the registry treats a static UFunction without AICallable as an error. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AIIgnore, DisplayName = "Get Dynamic Mesh"))
	static UDynamicMesh* GetDynamicMesh(int32 Handle);

	/** Store a natively built GeometryScriptMeshSelection on a handle under SelectionName, for the tools below that take one. Returns the selected count, or -1. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Set Selection"))
	static int32 SetSelection(int32 Handle, const FString& SelectionName, const FGeometryScriptMeshSelection& Selection);

	/** Drop every named selection stored on a handle. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Clear Selections"))
	static bool ClearSelections(int32 Handle);

	// =====================================================================
	// Primitives that honour MaterialID (5.9's generators ignore PrimitiveOptions.MaterialID for these)
	// =====================================================================

	/** Torus around the Z axis. MaterialID is stamped on the new triangles. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Append Torus"))
	static FModelingResult AppendTorus(int32 Handle, FTransform Transform, float MajorRadius = 50.f, float MinorRadius = 25.f, int32 MajorSteps = 16,
		int32 MinorSteps = 8, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	/** Revolve a 2D profile (X = radius offset, Y = height) around the Z axis: lathe shapes, rings, vases. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Append Revolve Polygon"))
	static FModelingResult AppendRevolvePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, float Radius = 100.f,
		int32 Steps = 16, float RevolveDegrees = 360.f, int32 MaterialID = 0);

	// =====================================================================
	// Assets
	// =====================================================================

	/**
	 * Write the mesh (with its bone weights) to a SkeletalMesh asset. SkeletonPath "" creates a new Skeleton asset at
	 * <AssetPath>_Skeleton from the bones on the mesh (CreateBones); an existing skeleton is bound as-is. One material slot
	 * is made per material ID — assign materials afterwards with SetAssetMaterials.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Save Mesh To Skeletal Mesh"))
	static FModelingResult SaveMeshToSkeletalMesh(int32 Handle, const FString& AssetPath, const FString& SkeletonPath = TEXT(""), bool bReplaceExisting = true, bool bSaveAsset = true);

	/** Assign material slots on a StaticMesh or SkeletalMesh asset: comma-separated material asset paths, in slot order; missing slots are added. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Set Asset Materials"))
	static FModelingResult SetAssetMaterials(const FString& AssetPath, const FString& MaterialPaths, bool bSaveAsset = true);

	// =====================================================================
	// Rigging: bones and skin weights authored on the mesh itself
	// =====================================================================

	/** Replace the mesh's bone hierarchy. The first bone is the root; every other bone names a parent listed before it. Every vertex starts bound to the root. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Create Bones"))
	static FModelingResult CreateBones(int32 Handle, const TArray<FModelingBoneDef>& Bones);

	/** Bind the vertices of a selection ("" = all) to a bone. Weight 1 replaces their weights; a lower weight blends with what is there. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Bind Selection To Bone"))
	static FModelingResult BindSelectionToBone(int32 Handle, const FString& SelectionName, const FString& BoneName, float Weight = 1.f);

	/** Select every vertex where the listed bones (comma-separated) together hold at least MinWeight of the skinning — the region a bone actually drags. Returns the vertex count. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Select By Bone Weight"))
	static int32 SelectByBoneWeight(int32 Handle, const FString& SelectionName, const FString& BoneNames, float MinWeight = 0.25f);

	/**
	 * Scale the listed bones' influence on a selection ("" = whole mesh): Scale 0 removes it, 0.25 keeps a quarter. The freed
	 * weight goes to ReceiverBoneName when given, otherwise to each vertex's strongest other influence. The targeted fix for
	 * clothing that follows the wrong bone — a collar lifting with the head, a skirt hem glued to a thigh. Vertices bound
	 * only to the listed bones are skipped unless a receiver is named.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Scale Bone Weights"))
	static FModelingResult ScaleBoneWeights(int32 Handle, const FString& SelectionName, const FString& BoneNames, float Scale = 0.f, const FString& ReceiverBoneName = TEXT(""));

	/** Laplacian-smooth skin weights across a selection ("" = whole mesh) — soften the seam after BindSelectionToBone or ScaleBoneWeights so a rigid patch blends into its surroundings. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Smooth Selection Bone Weights"))
	static FModelingResult SmoothSelectionBoneWeights(int32 Handle, const FString& SelectionName, int32 Iterations = 3, float Alpha = 0.5f);

	/** Per-bone skin weight report over a selection ("" = whole mesh): influenced vertices, average/max weight per bone, weight-sum range, max influences. Verify a repair before saving. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Inspect Bone Weights"))
	static FModelingResult InspectBoneWeights(int32 Handle, const FString& SelectionName = TEXT(""));

	// =====================================================================
	// UVs and textures
	// =====================================================================

	/** Islands, coverage, overlap and texel density of a UV layer — check an unwrap before baking or painting. GridResolution is the raster used for coverage/overlap. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Get UV Stats", ScriptName = "get_uv_stats"))
	static FModelingUVStats GetUVStats(int32 Handle, int32 UVLayer = 0, int32 GridResolution = 512);

	/** Tileable fractal (Perlin fBm) grayscale texture — grunge, wear masks, height detail. Scale is the number of noise cells across the image. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Create Noise Texture"))
	static FModelingResult CreateNoiseTexture(const FString& AssetPath, int32 Width = 1024, int32 Height = 1024, float Scale = 8.f, int32 Octaves = 4, float Persistence = 0.5f,
		int32 Seed = 0, bool bSaveAsset = true);

	/** Draw into an existing 8-bit texture in UV space (0-1, V down). Shape: "Line" (polyline through Points), "Dots" (discs every SpacingPx along the polyline), "Rect" (Points[0]..Points[1] filled), "Fill" (whole texture). */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Draw On Texture"))
	static FModelingResult DrawOnTexture(const FString& AssetPath, const FString& Shape, const TArray<FVector2D>& Points, FLinearColor Color, float ThicknessPx = 2.f,
		float SpacingPx = 8.f, bool bSaveAsset = true);

	// =====================================================================
	// Polylines
	// =====================================================================

	/** Stamp another session mesh every Spacing along a polyline, X axis along the line and Z along UpVector — rivet rows, fastener lines, light strips. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Append Mesh Along Polyline"))
	static FModelingResult AppendMeshAlongPolyline(int32 Handle, int32 OtherHandle, const TArray<FVector>& Points, float Spacing = 10.f, FVector UpVector = FVector::UpVector, float Scale = 1.f);

	/** Cut a rectangular groove (Width across, Depth into the surface along -UpVector, and as far out along +UpVector) following a polyline — recessed panel lines and seams. */
	UFUNCTION(BlueprintCallable, Category = "Vibe3D|Modeling", meta = (AICallable, DisplayName = "Cut Groove Along Polyline"))
	static FModelingResult CutGrooveAlongPolyline(int32 Handle, const TArray<FVector>& Points, float Width = 1.f, float Depth = 1.f, FVector UpVector = FVector::UpVector);
};
