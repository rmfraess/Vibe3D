// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Vibe3D/UModelingService.h"
#include "EditorAssetLibrary.h"
#include "Animation/Skeleton.h"
#include "AssetCompilingManager.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "UDynamicMesh.h"

// Self-provisioning: every test builds its own geometry, writes only under
// kModelingTestDir, and releases / deletes what it made. No project content is required.

static const EAutomationTestFlags kModelingTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
static const TCHAR* kModelingTestDir = TEXT("/Game/Developers/Vibe3DModelingTests");

namespace
{
	struct FScopedModelingSession
	{
		~FScopedModelingSession() { UModelingService::ReleaseAllMeshes(); }
	};

	int32 MakeBox(float Size = 100.f)
	{
		const int32 Handle = UModelingService::CreateMesh().Handle;
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(UModelingService::GetDynamicMesh(Handle), FGeometryScriptPrimitiveOptions(), FTransform::Identity, Size, Size, Size);
		return Handle;
	}
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingSessionTest, "Vibe3D.Modeling.Session", kModelingTestFlags)
bool FVibe3DModelingSessionTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FModelingResult Created = UModelingService::CreateMesh();
	TestTrue(TEXT("create succeeds"), Created.bSuccess);
	TestTrue(TEXT("handle is positive"), Created.Handle > 0);
	TestTrue(TEXT("dynamic mesh accessible"), UModelingService::GetDynamicMesh(Created.Handle) != nullptr);

	const FModelingResult Other = UModelingService::CreateMesh();
	TestEqual(TEXT("two meshes listed"), UModelingService::ListMeshes().Num(), 2);
	TestTrue(TEXT("release second mesh"), UModelingService::ReleaseMesh(Other.Handle));
	TestFalse(TEXT("release twice fails"), UModelingService::ReleaseMesh(Other.Handle));
	TestEqual(TEXT("release all count"), UModelingService::ReleaseAllMeshes(), 1);
	TestEqual(TEXT("nothing listed"), UModelingService::ListMeshes().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingErrorsTest, "Vibe3D.Modeling.Errors", kModelingTestFlags)
bool FVibe3DModelingErrorsTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FModelingResult NoHandle = UModelingService::AppendTorus(999, FTransform::Identity);
	TestFalse(TEXT("unknown handle fails"), NoHandle.bSuccess);
	TestTrue(TEXT("unknown handle explains"), NoHandle.Message.Contains(TEXT("create_mesh")));
	const int32 H = MakeBox();
	TestFalse(TEXT("empty mesh cannot be saved"), UModelingService::SaveMeshToSkeletalMesh(UModelingService::CreateMesh().Handle, TEXT("/Game/Developers/Nope"), TEXT(""), true, false).bSuccess);
	TestFalse(TEXT("relative asset path rejected"), UModelingService::SaveMeshToSkeletalMesh(H, TEXT("Nope"), TEXT(""), true, false).bSuccess);
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(UModelingService::GetDynamicMesh(H), Selection);
	TestEqual(TEXT("selection on unknown handle"), UModelingService::SetSelection(4242, TEXT("x"), Selection), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingRiggingTest, "Vibe3D.Modeling.LoftsRigging", kModelingTestFlags)
bool FVibe3DModelingRiggingTest::RunTest(const FString&)
{
	FScopedModelingSession Session;

	// Rig: a body box and a separate flap box, the flap bound to its own bone.
	const int32 H = MakeBox(100.f);
	UDynamicMesh* Mesh = UModelingService::GetDynamicMesh(H);
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(Mesh, FGeometryScriptPrimitiveOptions(), FTransform(FVector(0, 200, 0)), 50.f, 50.f, 50.f);
	FGeometryScriptMeshSelection FlapSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInBox(Mesh, FlapSelection, FBox(FVector(-30, 170, -1), FVector(30, 230, 51)));
	TestEqual(TEXT("set native flap selection"), UModelingService::SetSelection(H, TEXT("flap"), FlapSelection), 12);

	FModelingBoneDef Root;
	Root.Name = TEXT("root");
	FModelingBoneDef Flap;
	Flap.Name = TEXT("flap");
	Flap.ParentName = TEXT("root");
	Flap.Transform = FTransform(FVector(0, 175, 0));
	const FModelingResult Rig = UModelingService::CreateBones(H, { Root, Flap });
	TestTrue(FString::Printf(TEXT("create_bones: %s"), *Rig.Message), Rig.bSuccess);
	TestTrue(TEXT("bind flap"), UModelingService::BindSelectionToBone(H, TEXT("flap"), TEXT("flap"), 1.f).bSuccess);
	TestEqual(TEXT("native selection bound flap vertices"), UModelingService::SelectByBoneWeight(H, TEXT("weighted_flap"), TEXT("flap"), 0.99f), 8);
	TestEqual(TEXT("root retains body vertices"), UModelingService::SelectByBoneWeight(H, TEXT("weighted_body"), TEXT("root"), 0.99f), 8);
	TestFalse(TEXT("unknown bone rejected"), UModelingService::BindSelectionToBone(H, TEXT("flap"), TEXT("nope")).bSuccess);
	TestFalse(TEXT("child before parent rejected"), UModelingService::CreateBones(MakeBox(), { Flap, Root }).bSuccess);
	TestFalse(TEXT("stale selection rejected"), UModelingService::BindSelectionToBone(H, TEXT("missing"), TEXT("root")).bSuccess);
	TestTrue(TEXT("clear selections"), UModelingService::ClearSelections(H));
	TestFalse(TEXT("cleared selection rejected"), UModelingService::BindSelectionToBone(H, TEXT("flap"), TEXT("flap")).bSuccess);

	// Save as a skeletal mesh with a freshly created skeleton, assign materials, then clean up.
	const FString MeshPath = FString(kModelingTestDir) + TEXT("/SK_ModelingRig");
	const FModelingResult Saved = UModelingService::SaveMeshToSkeletalMesh(H, MeshPath, TEXT(""), true, false);
	TestTrue(FString::Printf(TEXT("save skeletal: %s"), *Saved.Message), Saved.bSuccess);
	TestTrue(TEXT("skeletal mesh asset exists"), UEditorAssetLibrary::DoesAssetExist(MeshPath));
	TestTrue(TEXT("skeleton asset created"), UEditorAssetLibrary::DoesAssetExist(MeshPath + TEXT("_Skeleton")));
	if (USkeleton* Skeleton = Cast<USkeleton>(UEditorAssetLibrary::LoadAsset(MeshPath + TEXT("_Skeleton"))))
	{
		TestEqual(TEXT("skeleton has both bones"), Skeleton->GetReferenceSkeleton().GetRawBoneNum(), 2);
		TestEqual(TEXT("flap bone in skeleton"), Skeleton->GetReferenceSkeleton().FindRawBoneIndex(TEXT("flap")), 1);
	}
	TestTrue(TEXT("set materials"), UModelingService::SetAssetMaterials(MeshPath, TEXT("/Engine/BasicShapes/BasicShapeMaterial"), false).bSuccess);
	TestFalse(TEXT("missing material rejected"), UModelingService::SetAssetMaterials(MeshPath, TEXT("/Game/Developers/NoSuchMaterial"), false).bSuccess);
	TestFalse(TEXT("skeletal save without weights rejected"), UModelingService::SaveMeshToSkeletalMesh(MakeBox(), FString(kModelingTestDir) + TEXT("/SK_NoWeights"), TEXT(""), true, false).bSuccess);
	FAssetCompilingManager::Get().FinishAllCompilation();
	UModelingService::ReleaseAllMeshes();
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingUVsTexturingTest, "Vibe3D.Modeling.UVsTexturing", kModelingTestFlags)
bool FVibe3DModelingUVsTexturingTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FString Dir = kModelingTestDir;

	// UV stats on a two-material box with one packed island per face.
	const int32 H = MakeBox(100.f);
	UDynamicMesh* Mesh = UModelingService::GetDynamicMesh(H);
	FGeometryScriptMeshSelection Top;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(Mesh, Top, FVector::UpVector, 5.f);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(Mesh, Top, 1);
	FGeometryScriptRecomputeUVsOptions UnwrapOptions;
	UnwrapOptions.IslandSource = EGeometryScriptUVIslandSource::PolyGroups;
	UGeometryScriptLibrary_MeshUVFunctions::RecomputeMeshUVs(Mesh, 0, UnwrapOptions, FGeometryScriptMeshSelection());
	FGeometryScriptLayoutUVsOptions LayoutOptions;
	LayoutOptions.TextureResolution = 512;
	UGeometryScriptLibrary_MeshUVFunctions::LayoutMeshUVs(Mesh, 0, LayoutOptions, FGeometryScriptMeshSelection());
	FModelingUVStats Stats = UModelingService::GetUVStats(H, 0, 256);
	TestTrue(FString::Printf(TEXT("uv stats: %s"), *Stats.Message), Stats.bSuccess);
	TestEqual(TEXT("one island per face"), Stats.NumIslands, 6);
	TestEqual(TEXT("no unset triangles"), Stats.NumUnsetTriangles, 0);
	TestTrue(TEXT("packed islands do not overlap"), Stats.OverlapFraction < 0.01f);
	TestTrue(TEXT("packed islands cover the square"), Stats.Coverage > 0.05f);
	TestTrue(TEXT("texel density reported"), Stats.TexelsPerCmAt1K > 0.f);
	UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(Mesh, 0, FVector2D(0.05, 0), Top);
	for (int32 MaterialID : { 0, 1 })
	{
		FGeometryScriptMeshSelection MaterialSelection;
		UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByMaterialID(Mesh, MaterialID, MaterialSelection);
		UGeometryScriptLibrary_MeshUVFunctions::LayoutMeshUVs(Mesh, 0, LayoutOptions, MaterialSelection);
	}
	Stats = UModelingService::GetUVStats(H, 0, 256);
	TestTrue(TEXT("per-material packs overlap each other by design"), Stats.OverlapFraction > 0.f);

	// Image tools: noise and drawing.
	const FString NoisePath = Dir + TEXT("/T_Noise");
	const FModelingResult Noise = UModelingService::CreateNoiseTexture(NoisePath, 64, 64, 4.f, 3, 0.5f, 7, false);
	TestTrue(FString::Printf(TEXT("noise: %s"), *Noise.Message), Noise.bSuccess);
	TestTrue(TEXT("noise asset exists"), UEditorAssetLibrary::DoesAssetExist(NoisePath));
	TestTrue(TEXT("draw line"), UModelingService::DrawOnTexture(NoisePath, TEXT("Line"), { FVector2D(0.1, 0.1), FVector2D(0.9, 0.9) }, FLinearColor::White, 3.f, 8.f, false).bSuccess);
	TestTrue(TEXT("draw dots"), UModelingService::DrawOnTexture(NoisePath, TEXT("Dots"), { FVector2D(0.1, 0.9), FVector2D(0.9, 0.1) }, FLinearColor::Black, 2.f, 6.f, false).bSuccess);
	TestTrue(TEXT("draw rect"), UModelingService::DrawOnTexture(NoisePath, TEXT("Rect"), { FVector2D(0.2, 0.2), FVector2D(0.4, 0.4) }, FLinearColor::Gray, 1.f, 1.f, false).bSuccess);
	TestFalse(TEXT("unknown shape rejected"), UModelingService::DrawOnTexture(NoisePath, TEXT("Spiral"), {}, FLinearColor::White).bSuccess);
	TestFalse(TEXT("draw on missing texture rejected"), UModelingService::DrawOnTexture(Dir + TEXT("/T_Nope"), TEXT("Fill"), {}, FLinearColor::White).bSuccess);

	// Surface detail: stamps and grooves.
	const int32 Rivet = MakeBox(2.f);
	const int32 RivetTris = UModelingService::GetDynamicMesh(Rivet)->GetTriangleCount();
	const int32 Before = Mesh->GetTriangleCount();
	const FModelingResult Row = UModelingService::AppendMeshAlongPolyline(H, Rivet, { FVector(-40, 20, 100), FVector(40, 20, 100) }, 20.f);
	TestTrue(FString::Printf(TEXT("stamp along polyline: %s"), *Row.Message), Row.bSuccess);
	TestEqual(TEXT("five stamps along 80 cm at 20 cm"), Mesh->GetTriangleCount(), Before + 5 * RivetTris);
	const int32 Slab = MakeBox(100.f);
	UDynamicMesh* SlabMesh = UModelingService::GetDynamicMesh(Slab);
	const int32 SlabBefore = SlabMesh->GetTriangleCount();
	const FModelingResult Groove = UModelingService::CutGrooveAlongPolyline(Slab, { FVector(-60, -30, 100), FVector(0, -30, 100), FVector(60, 0, 100) }, 2.f, 1.f);
	TestTrue(FString::Printf(TEXT("groove: %s"), *Groove.Message), Groove.bSuccess);
	TestTrue(TEXT("groove added geometry"), SlabMesh->GetTriangleCount() > SlabBefore);
	TestTrue(TEXT("grooved slab still closed"), UGeometryScriptLibrary_MeshQueryFunctions::GetIsClosedMesh(SlabMesh));
	TestFalse(TEXT("groove needs two points"), UModelingService::CutGrooveAlongPolyline(Slab, { FVector::ZeroVector }, 2.f, 1.f).bSuccess);

	FAssetCompilingManager::Get().FinishAllCompilation();
	UModelingService::ReleaseAllMeshes();
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Regressions found while building the showcase props (2026-08-30).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingMaterialIdRegressionTest, "Vibe3D.Modeling.MaterialIdOnAppend", kModelingTestFlags)
bool FVibe3DModelingMaterialIdRegressionTest::RunTest(const FString&)
{
	FScopedModelingSession Session;

	// AppendTorus / AppendRevolvePolygon ignore PrimitiveOptions.MaterialID, so the service stamps it afterwards.
	auto MaterialIds = [](int32 Handle)
	{
		FGeometryScriptIndexList Ids;
		bool bHasMaterialIDs = false;
		UGeometryScriptLibrary_MeshMaterialFunctions::GetAllTriangleMaterialIDs(UModelingService::GetDynamicMesh(Handle), Ids, bHasMaterialIDs);
		TSet<int32> Unique;
		if (bHasMaterialIDs && Ids.List.IsValid())
		{
			for (int32 Id : *Ids.List) { Unique.Add(Id); }
		}
		TArray<int32> Result = Unique.Array();
		Result.Sort();
		return Result;
	};
	auto OnlyMaterial = [this, &MaterialIds](int32 Handle, int32 Expected, const TCHAR* What)
	{
		const TArray<int32> Ids = MaterialIds(Handle);
		TestEqual(FString::Printf(TEXT("%s uses exactly one material id"), What), Ids.Num(), 1);
		if (Ids.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("%s honours material_id"), What), Ids[0], Expected);
		}
	};

	const int32 Torus = UModelingService::CreateMesh().Handle;
	const FModelingResult TorusResult = UModelingService::AppendTorus(Torus, FTransform::Identity, 40.f, 8.f, 16, 10, TEXT("Center"), 7);
	TestTrue(TEXT("torus succeeds and adds triangles"), TorusResult.bSuccess && TorusResult.TriangleCount > 0);
	OnlyMaterial(Torus, 7, TEXT("append_torus"));

	const int32 Revolve = UModelingService::CreateMesh().Handle;
	const FModelingResult RevolveResult = UModelingService::AppendRevolvePolygon(Revolve, FTransform::Identity,
		{ FVector2D(0, 0), FVector2D(20, 0), FVector2D(20, 40), FVector2D(0, 40) }, 0.f, 16, 360.f, 5);
	TestTrue(TEXT("revolve succeeds and adds triangles"), RevolveResult.bSuccess && RevolveResult.TriangleCount > 0);
	OnlyMaterial(Revolve, 5, TEXT("append_revolve_polygon"));

	// Stamping must only touch the triangles the append created, not the whole mesh.
	const int32 Mixed = MakeBox(50.f);
	UModelingService::AppendTorus(Mixed, FTransform(FVector(0, 0, 120)), 30.f, 6.f, 16, 10, TEXT("Center"), 2);
	TestEqual(TEXT("box keeps id 0 while the torus takes id 2"), MaterialIds(Mixed), TArray<int32>({ 0, 2 }));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
