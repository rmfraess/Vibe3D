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
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"

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

	// Native stand-ins for service tools this fork cut (UE 5.9 GeometryScript covers them), same defaults as the old tools.
	void AddBox(int32 Handle, const FTransform& Transform, float X, float Y, float Z, int32 StepsX = 0, int32 StepsY = 0, int32 StepsZ = 0, int32 MaterialID = 0)
	{
		FGeometryScriptPrimitiveOptions Options;
		Options.MaterialID = MaterialID;
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(UModelingService::GetDynamicMesh(Handle), Options, Transform, X, Y, Z, StepsX, StepsY, StepsZ, EGeometryScriptPrimitiveOriginMode::Base);
	}

	int32 MakeSphere(float Radius = 50.f, int32 StepsPhi = 12, int32 StepsTheta = 18)
	{
		const int32 Handle = UModelingService::CreateMesh().Handle;
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(UModelingService::GetDynamicMesh(Handle), FGeometryScriptPrimitiveOptions(), FTransform::Identity, Radius, StepsPhi, StepsTheta);
		return Handle;
	}

	int32 Tris(int32 Handle) { return UModelingService::GetDynamicMesh(Handle)->GetTriangleCount(); }

	FBox Bounds(int32 Handle)
	{
		FBox Box(ForceInit);
		UModelingService::GetDynamicMesh(Handle)->ProcessMesh([&Box](const UE::Geometry::FDynamicMesh3& M)
		{
			const UE::Geometry::FAxisAlignedBox3d B = M.GetBounds();
			Box = FBox(B.Min, B.Max);
		});
		return Box;
	}

	int32 CopyHandle(int32 Handle)
	{
		const int32 Copy = UModelingService::CreateMesh().Handle;
		UModelingService::GetDynamicMesh(Copy)->SetMesh(UModelingService::GetDynamicMesh(Handle)->GetMeshRef());
		return Copy;
	}

	/** Triangles of the connected piece nearest Point (the old select_connected). */
	TArray<int32> PieceTriangles(int32 Handle, const FVector& Point)
	{
		TArray<int32> Piece;
		UModelingService::GetDynamicMesh(Handle)->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& M)
		{
			int32 Seed = INDEX_NONE;
			double Best = TNumericLimits<double>::Max();
			for (const int32 Tid : M.TriangleIndicesItr())
			{
				const double D = FVector::DistSquared(M.GetTriCentroid(Tid), Point);
				if (D < Best) { Best = D; Seed = Tid; }
			}
			if (Seed == INDEX_NONE) { return; }
			TSet<int32> Seen = { Seed };
			Piece.Add(Seed);
			for (int32 i = 0; i < Piece.Num(); ++i)
			{
				const UE::Geometry::FIndex3i N = M.GetTriNeighbourTris(Piece[i]);
				for (int32 j = 0; j < 3; ++j)
				{
					if (N[j] != UE::Geometry::FDynamicMesh3::InvalidID && !Seen.Contains(N[j])) { Seen.Add(N[j]); Piece.Add(N[j]); }
				}
			}
		});
		return Piece;
	}

	/** The piece nearest Point as a selection, also stored on the handle under Name for service tools. */
	FGeometryScriptMeshSelection SelectPiece(int32 Handle, const FString& Name, const FVector& Point)
	{
		FGeometryScriptMeshSelection Selection;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(UModelingService::GetDynamicMesh(Handle), PieceTriangles(Handle, Point),
			EGeometryScriptMeshSelectionType::Triangles, Selection);
		UModelingService::SetSelection(Handle, Name, Selection);
		return Selection;
	}

	FBox PieceBounds(int32 Handle, const FVector& Point)
	{
		const TArray<int32> Piece = PieceTriangles(Handle, Point);
		FBox Box(ForceInit);
		UModelingService::GetDynamicMesh(Handle)->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& M)
		{
			for (const int32 Tid : Piece)
			{
				const UE::Geometry::FIndex3i T = M.GetTriangle(Tid);
				for (int32 j = 0; j < 3; ++j) { Box += M.GetVertex(T[j]); }
			}
		});
		return Box;
	}

	/** Vertices with a nonzero weight on Bone, -1 when the mesh has no such bone (the old list_bones' influenced_vertices). */
	int32 BoneVertexCount(int32 Handle, const FName Bone)
	{
		int32 Count = -1;
		UModelingService::GetDynamicMesh(Handle)->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& M)
		{
			if (!M.HasAttributes() || !M.Attributes()->HasBones()) { return; }
			const int32 Index = M.Attributes()->GetBoneNames()->GetAttribValues().IndexOfByKey(Bone);
			const UE::Geometry::FDynamicMeshVertexSkinWeightsAttribute* Skin = M.Attributes()->GetSkinWeightsAttribute(FName(TEXT("Default")));
			if (Index == INDEX_NONE || !Skin) { return; }
			Count = 0;
			UE::AnimationCore::FBoneWeights Weights;
			for (const int32 Vid : M.VertexIndicesItr())
			{
				Skin->GetValue(Vid, Weights);
				for (int32 i = 0; i < Weights.Num(); ++i) { if (Weights[i].GetBoneIndex() == Index && Weights[i].GetWeight() > 0.f) { ++Count; break; } }
			}
		});
		return Count;
	}

	int32 BoneCount(int32 Handle)
	{
		int32 Count = 0;
		UModelingService::GetDynamicMesh(Handle)->ProcessMesh([&Count](const UE::Geometry::FDynamicMesh3& M) { Count = M.HasAttributes() ? M.Attributes()->GetNumBones() : 0; });
		return Count;
	}

	bool SaveStatic(int32 Handle, const FString& AssetPath)
	{
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(UModelingService::GetDynamicMesh(Handle), AssetPath,
			FGeometryScriptCreateNewStaticMeshAssetOptions(), Outcome);
		return Outcome == EGeometryScriptOutcomePins::Success;
	}

	/**
	 * Write a minimal binary glTF the way an image-to-3D service delivers one: one node per session mesh (positions in
	 * metres, Y up, a planar UV), one material whose base colour is an embedded TextureSize^2 PNG. No content on disk needed.
	 */
	bool WriteTestGLB(const FString& Path, const TArray<int32>& Handles, int32 TextureSize)
	{
		TArray<uint8> Bin;
		const auto Align = [&Bin]() { while (Bin.Num() % 4) { Bin.Add(0); } };
		const auto Append = [&Bin](const void* Data, int32 Bytes) { Bin.Append(static_cast<const uint8*>(Data), Bytes); };
		TArray<FString> Accessors, Views, Meshes, Nodes;
		const auto AddView = [&](int32 Offset, int32 Length)
		{
			Views.Add(FString::Printf(TEXT("{\"buffer\":0,\"byteOffset\":%d,\"byteLength\":%d}"), Offset, Length));
			return Views.Num() - 1;
		};
		for (const int32 Handle : Handles)
		{
			TArray<FVector3f> Positions;
			TArray<FVector2f> UVs;
			TArray<uint32> Indices;
			UModelingService::GetDynamicMesh(Handle)->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& M)
			{
				TMap<int32, int32> Remap;
				for (const int32 Vid : M.VertexIndicesItr())
				{
					const FVector P = M.GetVertex(Vid);
					Remap.Add(Vid, Positions.Num());
					Positions.Add(FVector3f(P.X, P.Z, P.Y) / 100.f);   // UE cm Z-up -> glTF m Y-up
					UVs.Add(FVector2f(P.X / 200.f + 0.5f, P.Z / 200.f + 0.5f));
				}
				for (const int32 Tid : M.TriangleIndicesItr())
				{
					const UE::Geometry::FIndex3i T = M.GetTriangle(Tid);
					Indices.Append({ static_cast<uint32>(Remap[T.A]), static_cast<uint32>(Remap[T.C]), static_cast<uint32>(Remap[T.B]) });
				}
			});
			FBox3f Box(Positions);
			Align(); const int32 PosOffset = Bin.Num(); Append(Positions.GetData(), Positions.Num() * sizeof(FVector3f));
			Align(); const int32 UVOffset = Bin.Num(); Append(UVs.GetData(), UVs.Num() * sizeof(FVector2f));
			Align(); const int32 IdxOffset = Bin.Num(); Append(Indices.GetData(), Indices.Num() * sizeof(uint32));
			const int32 PosView = AddView(PosOffset, Positions.Num() * sizeof(FVector3f));
			const int32 UVView = AddView(UVOffset, UVs.Num() * sizeof(FVector2f));
			const int32 IdxView = AddView(IdxOffset, Indices.Num() * sizeof(uint32));
			const int32 First = Accessors.Num();
			Accessors.Add(FString::Printf(TEXT("{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\",\"min\":[%f,%f,%f],\"max\":[%f,%f,%f]}"),
				PosView, Positions.Num(), Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z));
			Accessors.Add(FString::Printf(TEXT("{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"VEC2\"}"), UVView, UVs.Num()));
			Accessors.Add(FString::Printf(TEXT("{\"bufferView\":%d,\"componentType\":5125,\"count\":%d,\"type\":\"SCALAR\"}"), IdxView, Indices.Num()));
			Meshes.Add(FString::Printf(TEXT("{\"primitives\":[{\"attributes\":{\"POSITION\":%d,\"TEXCOORD_0\":%d},\"indices\":%d,\"material\":0}]}"), First, First + 1, First + 2));
			Nodes.Add(FString::Printf(TEXT("{\"mesh\":%d,\"name\":\"Part%d\"}"), Meshes.Num() - 1, Meshes.Num() - 1));
		}
		TArray<FColor> Pixels;
		for (int32 Y = 0; Y < TextureSize; ++Y)
		{
			for (int32 X = 0; X < TextureSize; ++X) { Pixels.Add(((X / 8 + Y / 8) % 2) ? FColor(200, 60, 40) : FColor(240, 220, 200)); }
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(TextureSize, TextureSize, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		Align(); const int32 PngOffset = Bin.Num(); Append(Png.GetData(), static_cast<int32>(Png.Num()));
		const int32 PngView = AddView(PngOffset, static_cast<int32>(Png.Num()));
		Align();

		TArray<FString> NodeIds;
		for (int32 i = 0; i < Nodes.Num(); ++i) { NodeIds.Add(FString::FromInt(i)); }
		FString Json = FString::Printf(TEXT("{\"asset\":{\"version\":\"2.0\",\"generator\":\"Vibe3D test\"},\"scene\":0,\"scenes\":[{\"nodes\":[%s]}],\"nodes\":[%s],\"meshes\":[%s],")
			TEXT("\"materials\":[{\"name\":\"Generated\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicFactor\":0.0,\"roughnessFactor\":0.8}}],")
			TEXT("\"textures\":[{\"source\":0}],\"images\":[{\"bufferView\":%d,\"mimeType\":\"image/png\",\"name\":\"albedo\"}],")
			TEXT("\"accessors\":[%s],\"bufferViews\":[%s],\"buffers\":[{\"byteLength\":%d}]}"),
			*FString::Join(NodeIds, TEXT(",")), *FString::Join(Nodes, TEXT(",")), *FString::Join(Meshes, TEXT(",")), PngView,
			*FString::Join(Accessors, TEXT(",")), *FString::Join(Views, TEXT(",")), Bin.Num());
		FTCHARToUTF8 Utf8(*Json);
		TArray<uint8> JsonBytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		while (JsonBytes.Num() % 4) { JsonBytes.Add(' '); }

		TArray<uint8> Out;
		const auto Put32 = [&Out](uint32 V) { Out.Append(reinterpret_cast<const uint8*>(&V), 4); };
		Put32(0x46546C67); Put32(2); Put32(12 + 8 + JsonBytes.Num() + 8 + Bin.Num());
		Put32(JsonBytes.Num()); Put32(0x4E4F534A); Out.Append(JsonBytes);
		Put32(Bin.Num()); Put32(0x004E4942); Out.Append(Bin);
		return FFileHelper::SaveArrayToFile(Out, *Path);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingVertexArraysTest, "Vibe3D.Modeling.VertexArrays", kModelingTestFlags)
bool FVibe3DModelingVertexArraysTest::RunTest(const FString&)
{
	FScopedModelingSession Session;

	// Round trip: read, move, write back; the layout is indexed by vertex ID.
	const int32 H = MakeBox(100.f);
	TArray<FVector> Positions = UModelingService::GetVertexPositions(H);
	TestEqual(TEXT("one entry per vertex"), Positions.Num(), 8);
	for (FVector& P : Positions) { P.Z += 10.0; }
	const FModelingResult Set = UModelingService::SetVertexPositions(H, Positions);
	TestTrue(FString::Printf(TEXT("set succeeds: %s"), *Set.Message), Set.bSuccess);
	TestTrue(TEXT("bounds moved up"), FMath::IsNearlyEqual(Bounds(H).Min.Z, 10.0, 0.001));
	TestFalse(TEXT("wrong length rejected"), UModelingService::SetVertexPositions(H, { FVector::ZeroVector }).bSuccess);
	TestEqual(TEXT("unknown handle gives an empty list"), UModelingService::GetVertexPositions(4242).Num(), 0);

	// A mesh with ID gaps keeps its slots: the list is MaxVertexID long and gap slots are ignored on write.
	const int32 G = MakeBox(100.f);
	AddBox(G, FTransform(FVector(300, 0, 0)), 50.f, 50.f, 50.f);
	int32 Deleted = 0;
	UGeometryScriptLibrary_MeshBasicEditFunctions::DeleteSelectedTrianglesFromMesh(UModelingService::GetDynamicMesh(G), SelectPiece(G, TEXT("first"), FVector(0, 0, 50)), Deleted);
	TArray<FVector> GapPositions = UModelingService::GetVertexPositions(G);
	TestTrue(TEXT("gap layout is at least as long as the vertex count"), GapPositions.Num() >= UModelingService::GetDynamicMesh(G)->GetMeshRef().VertexCount());
	for (FVector& P : GapPositions) { P.X -= 300.0; }
	TestTrue(TEXT("gap layout accepted"), UModelingService::SetVertexPositions(G, GapPositions).bSuccess);
	TestTrue(TEXT("remaining box moved to the origin"), FMath::IsNearlyEqual(Bounds(G).Min.X, -25.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingIntakeTest, "Vibe3D.Modeling.IntakeGeneratedMesh", kModelingTestFlags)
bool FVibe3DModelingIntakeTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FString Folder = FString(kModelingTestDir) / TEXT("Intake");
	const FString AssetPath = Folder / TEXT("SM_TestProp");
	if (UEditorAssetLibrary::DoesDirectoryExist(kModelingTestDir)) { UEditorAssetLibrary::DeleteDirectory(kModelingTestDir); }

	// A generated "prop": a sphere and a separate tall box, two meshes in one GLB, a 64 px texture. 150 cm tall as delivered.
	const int32 Sphere = MakeSphere(50.f);
	const int32 Box = UModelingService::CreateMesh().Handle;
	AddBox(Box, FTransform(FVector(100, 0, 0)), 40.f, 40.f, 100.f, 2, 2, 6);
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Vibe3DTests/intake_test.glb"));
	TestTrue(TEXT("test GLB written"), WriteTestGLB(File, { Sphere, Box }, 64));

	const FModelingIntakeReport First = UModelingService::IntakeGeneratedMesh(File, AssetPath, 75.f, TEXT("Height"), 0.f, TEXT("BottomCenter"),
		200, 32, TEXT("ConvexHulls"), 4, TEXT("1,0.5"));
	TestTrue(FString::Printf(TEXT("intake succeeds: %s"), *First.Message), First.bSuccess);
	AddInfo(First.Message);
	TestTrue(TEXT("asset exists"), UEditorAssetLibrary::DoesAssetExist(AssetPath));
	TestTrue(TEXT("both meshes read"), First.SourceMeshCount >= 1 && First.SourceTriangles > 200);
	TestTrue(FString::Printf(TEXT("decimated to the budget (%d)"), First.TriangleCount), First.TriangleCount > 0 && First.TriangleCount <= 210);
	TestTrue(FString::Printf(TEXT("fitted to 75 cm tall (%s..%s)"), *First.BoundsMin.ToString(), *First.BoundsMax.ToString()),
		FMath::IsNearlyEqual(First.BoundsMax.Z - First.BoundsMin.Z, 75.0, 1.0));
	TestTrue(TEXT("pivot at the floor"), FMath::IsNearlyEqual(First.BoundsMin.Z, 0.0, 0.5));
	TestTrue(TEXT("pivot centred in XY"), FMath::Abs(First.BoundsMin.X + First.BoundsMax.X) < 1.0 && FMath::Abs(First.BoundsMin.Y + First.BoundsMax.Y) < 1.0);
	TestTrue(TEXT("uniform scale"), FMath::IsNearlyEqual(First.ScaleApplied.X, First.ScaleApplied.Z, 1e-4));
	TestEqual(TEXT("two LODs"), First.LODCount, 2);
	TestTrue(TEXT("collision built"), First.CollisionShapes >= 1);
	TestEqual(TEXT("one material slot"), First.MaterialPaths.Num(), 1);
	if (First.MaterialPaths.Num() == 1) { TestTrue(FString::Printf(TEXT("material moved next to the mesh (%s)"), *First.MaterialPaths[0]), First.MaterialPaths[0].StartsWith(Folder / TEXT("M"))); }
	TestEqual(TEXT("one texture"), First.Textures.Num(), 1);
	if (First.Textures.Num() == 1) { TestTrue(FString::Printf(TEXT("texture capped (%s)"), *First.Textures[0]), First.Textures[0].EndsWith(TEXT("max 32")) && First.Textures[0].StartsWith(Folder / TEXT("T_TestProp_"))); }
	TestFalse(TEXT("staging folder removed"), UEditorAssetLibrary::DoesDirectoryExist(Folder / TEXT("_Intake_TestProp")));
	int32 StaticMeshes = 0;
	for (const FString& Path : UEditorAssetLibrary::ListAssets(Folder, true, false))
	{
		if (Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Path))) { ++StaticMeshes; }
	}
	TestEqual(TEXT("exactly one static mesh kept"), StaticMeshes, 1);
	UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, *AssetPath);

	// Same file and settings: nothing to do.
	const FModelingIntakeReport Again = UModelingService::IntakeGeneratedMesh(File, AssetPath, 75.f, TEXT("Height"), 0.f, TEXT("BottomCenter"),
		200, 32, TEXT("ConvexHulls"), 4, TEXT("1,0.5"));
	TestTrue(FString::Printf(TEXT("second run is up to date: %s"), *Again.Message), Again.bSuccess && Again.bUpToDate);

	// New settings replace the asset in place (same object, so placed actors keep it) without piling up materials.
	const FModelingIntakeReport Boxed = UModelingService::IntakeGeneratedMesh(File, AssetPath, 0.f, TEXT("Box"), 90.f, TEXT("Center"),
		0, 0, TEXT("Box"), 1, TEXT(""), FVector(60, 30, 90));
	TestTrue(FString::Printf(TEXT("box fit succeeds: %s"), *Boxed.Message), Boxed.bSuccess && !Boxed.bUpToDate);
	AddInfo(Boxed.Message);
	TestTrue(TEXT("replaced in place"), LoadObject<UStaticMesh>(nullptr, *AssetPath) == Asset);
	const FVector BoxSize = Boxed.BoundsMax - Boxed.BoundsMin;
	TestTrue(FString::Printf(TEXT("stretched to the box (%s)"), *BoxSize.ToString()), BoxSize.Equals(FVector(60, 30, 90), 1.0));
	TestTrue(TEXT("centre pivot"), (Boxed.BoundsMax + Boxed.BoundsMin).IsNearlyZero(1.0));
	TestEqual(TEXT("LODs cleared back to one"), Boxed.LODCount, 1);
	TestFalse(TEXT("no duplicate material"), UEditorAssetLibrary::DoesAssetExist(Folder / TEXT("MI_TestProp_0_2")) || UEditorAssetLibrary::DoesAssetExist(Folder / TEXT("M_TestProp_0_2")));

	// Somebody else's assets are not intake's to replace: a mesh at the target path needs force, and unreferenced assets
	// that happen to have the names intake wants (no intake tag) are kept, the new ones get a numbered name.
	const FString OtherFolder = FString(kModelingTestDir) / TEXT("IntakeOther");
	const FString OtherPath = OtherFolder / TEXT("SM_TestProp");
	TestTrue(TEXT("a hand-made mesh at the target path"), SaveStatic(MakeBox(), OtherPath));
	TArray<FString> Decoys;
	for (const FString& Made : First.MaterialPaths) { Decoys.Add(OtherFolder / FPackageName::GetShortName(FPackageName::ObjectPathToPackageName(Made))); }
	for (const FString& Made : First.Textures) { Decoys.Add(OtherFolder / FPackageName::GetShortName(FPackageName::ObjectPathToPackageName(Made.Left(Made.Find(TEXT(" ")))))); }
	for (const FString& Decoy : Decoys) { TestTrue(FString::Printf(TEXT("decoy %s"), *Decoy), SaveStatic(MakeBox(), Decoy)); }
	const FModelingIntakeReport Refused = UModelingService::IntakeGeneratedMesh(File, OtherPath, 75.f);
	TestTrue(FString::Printf(TEXT("foreign mesh not replaced without force: %s"), *Refused.Message), !Refused.bSuccess && Refused.Message.Contains(TEXT("force")));
	TestTrue(TEXT("foreign mesh untouched"), LoadObject<UStaticMesh>(nullptr, *OtherPath)->GetNumTriangles(0) == 12);
	const FModelingIntakeReport Forced = UModelingService::IntakeGeneratedMesh(File, OtherPath, 75.f, TEXT("Height"), 0.f, TEXT("BottomCenter"),
		200, 32, TEXT("None"), 1, TEXT(""), FVector::ZeroVector, false, true);
	TestTrue(FString::Printf(TEXT("force replaces it: %s"), *Forced.Message), Forced.bSuccess && !Forced.bUpToDate);
	for (const FString& Decoy : Decoys) { TestNotNull(*FString::Printf(TEXT("untagged %s kept"), *Decoy), Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Decoy))); }
	for (const FString& Made : Forced.MaterialPaths) { TestFalse(FString::Printf(TEXT("new material got its own name (%s)"), *Made), Decoys.Contains(FPackageName::ObjectPathToPackageName(Made))); }
	TestTrue(TEXT("once intake's, re-running needs no force"), UModelingService::IntakeGeneratedMesh(File, OtherPath, 60.f).bSuccess);

	// Refusals.
	TestFalse(TEXT("missing file rejected"), UModelingService::IntakeGeneratedMesh(File + TEXT(".missing"), AssetPath).bSuccess);
	TestFalse(TEXT("unknown fit rejected"), UModelingService::IntakeGeneratedMesh(File, AssetPath, 50.f, TEXT("Diagonal")).bSuccess);
	TestFalse(TEXT("box fit without a box rejected"), UModelingService::IntakeGeneratedMesh(File, AssetPath, 50.f, TEXT("Box")).bSuccess);
	TestFalse(TEXT("bad LOD list rejected"), UModelingService::IntakeGeneratedMesh(File, AssetPath, 50.f, TEXT("Height"), 0.f, TEXT("BottomCenter"), 0, 0, TEXT("None"), 1, TEXT("1,2")).bSuccess);

	FAssetCompilingManager::Get().FinishAllCompilation();
	UModelingService::ReleaseAllMeshes();
	IFileManager::Get().Delete(*File);
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

namespace
{
	TArray<UMaterialInterface*> LODMaterials(USkeletalMesh* SkeletalMesh, int32 LODIndex)
	{
		TArray<UMaterialInterface*> Materials;
		TArray<int32> Indices;
		TArray<FName> Names;
		FGeometryScriptMeshReadLOD ReadLOD;
		ReadLOD.LODIndex = LODIndex;
		EGeometryScriptOutcomePins Outcome;
		UGeometryScriptLibrary_StaticMeshFunctions::GetLODMaterialListFromSkeletalMesh(SkeletalMesh, ReadLOD, Materials, Indices, Names, Outcome);
		return Materials;
	}

	/** A two-bone, two-material skeletal mesh asset: a subdivided body box (root, material 0) and a flap box (flap, material 1). */
	int32 MakeRiggedTwoMaterialMesh()
	{
		const int32 H = UModelingService::CreateMesh().Handle;
		AddBox(H, FTransform::Identity, 100.f, 100.f, 100.f, 4, 4, 4);
		AddBox(H, FTransform(FVector(0, 200, 0)), 50.f, 50.f, 50.f, 2, 2, 2, 1);
		SelectPiece(H, TEXT("flap"), FVector(0, 200, 25));
		FModelingBoneDef Root;
		Root.Name = TEXT("root");
		FModelingBoneDef Flap;
		Flap.Name = TEXT("flap");
		Flap.ParentName = TEXT("root");
		Flap.Transform = FTransform(FVector(0, 175, 0));
		UModelingService::CreateBones(H, { Root, Flap });
		UModelingService::BindSelectionToBone(H, TEXT("flap"), TEXT("flap"), 1.f);
		return H;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingSkeletalLODTest, "Vibe3D.Modeling.SkeletalLODRoundTrip", kModelingTestFlags)
bool FVibe3DModelingSkeletalLODTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	if (UEditorAssetLibrary::DoesDirectoryExist(kModelingTestDir)) { UEditorAssetLibrary::DeleteDirectory(kModelingTestDir); }
	const FString MeshPath = FString(kModelingTestDir) / TEXT("SK_LODRoundTrip");

	const int32 H = MakeRiggedTwoMaterialMesh();
	TestTrue(TEXT("asset saved"), UModelingService::SaveMeshToSkeletalMesh(H, MeshPath, TEXT(""), true, false).bSuccess);
	TestTrue(TEXT("two slots assigned"), UModelingService::SetAssetMaterials(MeshPath,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial,/Engine/EngineMaterials/WorldGridMaterial"), false).bSuccess);
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!TestNotNull(TEXT("skeletal mesh"), SkeletalMesh)) { return false; }
	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

	// Append LOD1 from a simplified copy.
	const int32 Low = CopyHandle(H);
	UModelingService::SimplifyToTriangleCount(Low, Tris(H) / 2);
	const FModelingResult Added = UModelingService::SaveMeshToSkeletalMeshLOD(Low, MeshPath, 1, false);
	TestTrue(FString::Printf(TEXT("LOD1 appended: %s"), *Added.Message), Added.bSuccess);
	TestEqual(TEXT("two LODs"), SkeletalMesh->GetLODNum(), 2);
	const FModelingResult Inspected = UModelingService::InspectSkeletalMesh(MeshPath);
	TestTrue(FString::Printf(TEXT("inspect lists both LODs: %s"), *Inspected.Message), Inspected.bSuccess && Inspected.Message.Contains(TEXT("LOD 1:")));
	AddInfo(Inspected.Message);
	const TArray<UMaterialInterface*> LOD0Before = LODMaterials(SkeletalMesh, 0);
	const TArray<UMaterialInterface*> LOD1Before = LODMaterials(SkeletalMesh, 1);
	const int32 LOD0Verts = UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 0).VertexCount;

	// Round trip LOD1 in place: load, move up 10 cm, save with no path.
	const FModelingResult Loaded = UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 1);
	TestTrue(FString::Printf(TEXT("load LOD1: %s"), *Loaded.Message), Loaded.bSuccess && Loaded.Message.Contains(TEXT("source model")));
	UModelingService::TranslateMesh(Loaded.Handle, FVector(0, 0, 10));
	const FModelingResult Saved = UModelingService::SaveMeshToSkeletalMeshLOD(Loaded.Handle);
	TestTrue(FString::Printf(TEXT("LOD1 written back: %s"), *Saved.Message), Saved.bSuccess);
	TestTrue(TEXT("unchanged topology keeps the vertex order"), Saved.Message.Contains(TEXT("original vertex order")));
	AddInfo(Saved.Message);
	TestEqual(TEXT("still two LODs"), SkeletalMesh->GetLODNum(), 2);
	TestTrue(TEXT("same skeleton"), SkeletalMesh->GetSkeleton() == Skeleton);
	TestEqual(TEXT("still two bones"), SkeletalMesh->GetRefSkeleton().GetRawBoneNum(), 2);
	TestEqual(TEXT("still two slots"), SkeletalMesh->GetMaterials().Num(), 2);
	TestTrue(TEXT("LOD1 sections keep their materials"), LODMaterials(SkeletalMesh, 1) == LOD1Before);
	const FModelingResult LOD1After = UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 1);
	TestTrue(TEXT("LOD1 moved up"), FMath::IsNearlyEqual(Bounds(LOD1After.Handle).Min.Z, 10.0, 0.01));
	TestEqual(TEXT("LOD1 vertex count kept"), LOD1After.VertexCount, Loaded.VertexCount);
	const FModelingResult LOD0After = UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 0);
	TestTrue(TEXT("LOD0 untouched"), FMath::IsNearlyEqual(Bounds(LOD0After.Handle).Min.Z, 0.0, 0.01) && LOD0After.VertexCount == LOD0Verts);

	// LOD0 round trip through the old entry point keeps the slots too.
	UModelingService::TranslateMesh(LOD0After.Handle, FVector(5, 0, 0));
	TestTrue(TEXT("LOD0 replaced via save_mesh_to_skeletal_mesh"), UModelingService::SaveMeshToSkeletalMesh(LOD0After.Handle, MeshPath, TEXT(""), true, false).bSuccess);
	TestTrue(TEXT("LOD0 sections keep their materials"), LODMaterials(SkeletalMesh, 0) == LOD0Before);
	TestEqual(TEXT("LOD1 survives a LOD0 write"), SkeletalMesh->GetLODNum(), 2);

	// Refusals: nothing is written.
	TestFalse(TEXT("no weights rejected"), UModelingService::SaveMeshToSkeletalMeshLOD(MakeBox(), MeshPath, 1, false).bSuccess);
	const int32 Foreign = MakeBox();
	FModelingBoneDef Root;
	Root.Name = TEXT("root");
	FModelingBoneDef Bogus;
	Bogus.Name = TEXT("bogus");
	Bogus.ParentName = TEXT("root");
	UModelingService::CreateBones(Foreign, { Root, Bogus });
	UModelingService::BindSelectionToBone(Foreign, TEXT(""), TEXT("bogus"), 1.f);
	const FModelingResult ForeignResult = UModelingService::SaveMeshToSkeletalMeshLOD(Foreign, MeshPath, 1, false);
	TestTrue(FString::Printf(TEXT("foreign bone rejected: %s"), *ForeignResult.Message), !ForeignResult.bSuccess && ForeignResult.Message.Contains(TEXT("bogus")));
	const int32 BadSlot = UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 1).Handle;
	UGeometryScriptLibrary_MeshMaterialFunctions::ClearMaterialIDs(UModelingService::GetDynamicMesh(BadSlot), 7);
	const FModelingResult SlotResult = UModelingService::SaveMeshToSkeletalMeshLOD(BadSlot);
	TestTrue(FString::Printf(TEXT("material without a slot rejected: %s"), *SlotResult.Message), !SlotResult.bSuccess && SlotResult.Message.Contains(TEXT("Material ID 7")));
	const int32 Unweighted = UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 1).Handle;
	bool bValidVertex = false;
	UGeometryScriptLibrary_MeshBoneWeightFunctions::SetVertexBoneWeights(UModelingService::GetDynamicMesh(Unweighted), 0, {}, bValidVertex);
	const FModelingResult UnweightedResult = UModelingService::SaveMeshToSkeletalMeshLOD(Unweighted);
	TestTrue(FString::Printf(TEXT("unweighted vertex rejected: %s"), *UnweightedResult.Message), !UnweightedResult.bSuccess && UnweightedResult.Message.Contains(TEXT("no bone weight")));
	TestFalse(TEXT("LOD beyond the end rejected"), UModelingService::SaveMeshToSkeletalMeshLOD(BadSlot, MeshPath, 5, false).bSuccess);
	TestFalse(TEXT("missing LOD read rejected"), UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 9).bSuccess);
	TestFalse(TEXT("unknown origin needs a path"), UModelingService::SaveMeshToSkeletalMeshLOD(MakeBox()).bSuccess);
	TestEqual(TEXT("refusals left two LODs"), SkeletalMesh->GetLODNum(), 2);

	FAssetCompilingManager::Get().FinishAllCompilation();
	UModelingService::ReleaseAllMeshes();
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingAccessoryReskinTest, "Vibe3D.Modeling.AccessoryReskin", kModelingTestFlags)
bool FVibe3DModelingAccessoryReskinTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	if (UEditorAssetLibrary::DoesDirectoryExist(kModelingTestDir)) { UEditorAssetLibrary::DeleteDirectory(kModelingTestDir); }
	const FString BodyPath = FString(kModelingTestDir) / TEXT("SK_ReskinBody");
	TestTrue(TEXT("body saved"), UModelingService::SaveMeshToSkeletalMesh(MakeRiggedTwoMaterialMesh(), BodyPath, TEXT(""), true, false).bSuccess);
	const int32 Body = UModelingService::LoadMeshFromSkeletalMesh(BodyPath, 0).Handle;

	const auto BoneVertices = [](int32 Handle, const TCHAR* Bone) { return BoneVertexCount(Handle, FName(Bone)); };

	// A plate lying on the flap follows the flap.
	const int32 Plate = UModelingService::CreateMesh().Handle;
	AddBox(Plate, FTransform(FVector(0, 200, 50)), 20.f, 20.f, 2.f);
	const FModelingResult Surface = UModelingService::SkinToSurface(Plate, Body);
	TestTrue(FString::Printf(TEXT("surface skin: %s"), *Surface.Message), Surface.bSuccess);
	TestEqual(TEXT("plate carries the body's two bones"), BoneCount(Plate), 2);
	TestEqual(TEXT("every plate vertex on the flap"), BoneVertices(Plate, TEXT("flap")), 8);
	TestEqual(TEXT("none on the root"), BoneVertices(Plate, TEXT("root")), 0);

	// Rigid: one bone for the whole piece, chosen from the surface or named.
	const int32 Badge = UModelingService::CreateMesh().Handle;
	AddBox(Badge, FTransform(FVector(0, 0, 100)), 20.f, 20.f, 2.f);
	const FModelingResult Rigid = UModelingService::SkinToSurface(Badge, Body, TEXT("Rigid"));
	TestTrue(FString::Printf(TEXT("rigid picks the bone underneath: %s"), *Rigid.Message), Rigid.bSuccess && Rigid.Message.Contains(TEXT("'root'")));
	TestEqual(TEXT("rigid binds every vertex"), BoneVertices(Badge, TEXT("root")), 8);
	TestTrue(TEXT("rigid to a named bone"), UModelingService::SkinToSurface(Badge, Body, TEXT("Rigid"), TEXT("flap")).bSuccess);
	TestEqual(TEXT("named bone took it all"), BoneVertices(Badge, TEXT("flap")), 8);
	TestFalse(TEXT("unknown rigid bone rejected"), UModelingService::SkinToSurface(Badge, Body, TEXT("Rigid"), TEXT("nope")).bSuccess);
	TestTrue(TEXT("inpaint mode"), UModelingService::SkinToSurface(Badge, Body, TEXT("Inpaint")).bSuccess);
	TestTrue(TEXT("bones from an asset"), UModelingService::SkinToSurface(Plate, Body, TEXT("Surface"), TEXT(""), BodyPath).bSuccess);
	TestFalse(TEXT("unskinned source rejected"), UModelingService::SkinToSurface(Plate, MakeBox()).bSuccess);
	TestFalse(TEXT("unknown mode rejected"), UModelingService::SkinToSurface(Plate, Body, TEXT("Glue")).bSuccess);

	// The skinned plate saves as a follower mesh on the body's skeleton.
	USkeletalMesh* BodyAsset = LoadObject<USkeletalMesh>(nullptr, *BodyPath);
	const FString PlatePath = FString(kModelingTestDir) / TEXT("SKM_Plate");
	const FModelingResult PlateSaved = UModelingService::SaveMeshToSkeletalMesh(Plate, PlatePath, BodyAsset->GetSkeleton()->GetPathName(), true, false);
	TestTrue(FString::Printf(TEXT("plate saved on the body skeleton: %s"), *PlateSaved.Message), PlateSaved.bSuccess);

	// LOD bone remap: the reference (leader) drops flap_tip at LOD1; a follower weighted to flap_tip must move to flap.
	const auto MakeThreeBoneMesh = [](const TCHAR* FlapBone)
	{
		const int32 H = UModelingService::CreateMesh().Handle;
		AddBox(H, FTransform::Identity, 100.f, 100.f, 100.f);
		AddBox(H, FTransform(FVector(0, 200, 0)), 50.f, 50.f, 50.f);
		SelectPiece(H, TEXT("flap"), FVector(0, 200, 25));
		FModelingBoneDef Root; Root.Name = TEXT("root");
		FModelingBoneDef Flap; Flap.Name = TEXT("flap"); Flap.ParentName = TEXT("root"); Flap.Transform = FTransform(FVector(0, 175, 0));
		FModelingBoneDef Tip; Tip.Name = TEXT("flap_tip"); Tip.ParentName = TEXT("flap"); Tip.Transform = FTransform(FVector(0, 225, 0));
		UModelingService::CreateBones(H, { Root, Flap, Tip });
		UModelingService::BindSelectionToBone(H, TEXT("flap"), FlapBone, 1.f);
		return H;
	};
	const FString RefPath = FString(kModelingTestDir) / TEXT("SK_Leader");
	const int32 Leader = MakeThreeBoneMesh(TEXT("flap"));
	TestTrue(TEXT("leader saved"), UModelingService::SaveMeshToSkeletalMesh(Leader, RefPath, TEXT(""), true, false).bSuccess);
	TestTrue(TEXT("leader LOD1 added"), UModelingService::SaveMeshToSkeletalMeshLOD(CopyHandle(Leader), RefPath, 1, false).bSuccess);
	USkeletalMesh* LeaderAsset = LoadObject<USkeletalMesh>(nullptr, *RefPath);
	LeaderAsset->Modify();
	FBoneReference Removed;
	Removed.BoneName = TEXT("flap_tip");
	LeaderAsset->GetLODInfo(1)->BonesToRemove.Add(Removed);
	LeaderAsset->PostEditChange();
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FModelingResult LeaderInfo = UModelingService::InspectSkeletalMesh(RefPath);
	AddInfo(LeaderInfo.Message);
	TestTrue(TEXT("the leader drops a bone at LOD1"), LeaderInfo.Message.Contains(TEXT("LOD 1: source model")) && LeaderInfo.Message.Contains(TEXT("2 bone(s) used at runtime")));

	const FString FollowerPath = FString(kModelingTestDir) / TEXT("SKM_Follower");
	TestTrue(TEXT("follower saved"), UModelingService::SaveMeshToSkeletalMesh(MakeThreeBoneMesh(TEXT("flap_tip")), FollowerPath, TEXT(""), true, false).bSuccess);
	const FModelingResult Remap = UModelingService::RemapLODBones(FollowerPath, RefPath, false);
	TestTrue(FString::Printf(TEXT("remap: %s"), *Remap.Message), Remap.bSuccess && Remap.VertexCount == 8 && Remap.Message.Contains(TEXT("flap_tip->flap")));
	AddInfo(Remap.Message);
	const int32 Remapped = UModelingService::LoadMeshFromSkeletalMesh(FollowerPath, 0).Handle;
	TestEqual(TEXT("nothing left on the dropped bone"), BoneVertices(Remapped, TEXT("flap_tip")), 0);
	TestEqual(TEXT("its parent took over"), BoneVertices(Remapped, TEXT("flap")), 8);
	TestEqual(TEXT("second remap is a no-op"), UModelingService::RemapLODBones(FollowerPath, RefPath, false).VertexCount, 0);
	TestFalse(TEXT("missing reference rejected"), UModelingService::RemapLODBones(FollowerPath, TEXT("/Game/Developers/NoSuchMesh"), false).bSuccess);

	FAssetCompilingManager::Get().FinishAllCompilation();
	UModelingService::ReleaseAllMeshes();
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingPosedComponentTest, "Vibe3D.Modeling.PosedComponent", kModelingTestFlags)
bool FVibe3DModelingPosedComponentTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	if (UEditorAssetLibrary::DoesDirectoryExist(kModelingTestDir)) { UEditorAssetLibrary::DeleteDirectory(kModelingTestDir); }
	const FString MeshPath = FString(kModelingTestDir) / TEXT("SK_PoseTest");
	TestTrue(TEXT("asset saved"), UModelingService::SaveMeshToSkeletalMesh(MakeRiggedTwoMaterialMesh(), MeshPath, TEXT(""), true, false).bSuccess);
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World) || !SkeletalMesh) { return false; }

	// A placed character stand-in: a poseable "body" with the flap bone raised 30 cm, and a leader-pose follower.
	AActor* Actor = World->SpawnActor<AActor>(FVector(1000, 0, 0), FRotator::ZeroRotator);
	UPoseableMeshComponent* Pose = NewObject<UPoseableMeshComponent>(Actor, TEXT("Pose"));
	Actor->SetRootComponent(Pose);
	Pose->SetSkinnedAssetAndUpdate(SkeletalMesh);
	Pose->RegisterComponent();
	USkeletalMeshComponent* Follower = NewObject<USkeletalMeshComponent>(Actor, TEXT("Follower"));
	Follower->SetupAttachment(Pose);
	Follower->SetSkinnedAssetAndUpdate(SkeletalMesh);
	Follower->RegisterComponent();
	Follower->SetLeaderPoseComponent(Pose);
	Actor->SetActorLocation(FVector(1000, 0, 0));
	Actor->SetActorLabel(TEXT("Vibe3DPoseTest"));
	Pose->SetBoneLocationByName(TEXT("flap"), FVector(0, 175, 30), EBoneSpaces::ComponentSpace);
	Pose->RefreshBoneTransforms();

	const auto PieceMinZ = [](int32 Handle, const FVector& Point) { return PieceBounds(Handle, Point).Min.Z; };

	const FModelingResult Posed = UModelingService::LoadMeshFromSkinnedComponent(TEXT("Vibe3DPoseTest"), TEXT("Pose"), false, 0, true);
	TestTrue(FString::Printf(TEXT("posed read: %s"), *Posed.Message), Posed.bSuccess);
	AddInfo(Posed.Message);
	TestTrue(TEXT("flap raised in the pose"), FMath::IsNearlyEqual(PieceMinZ(Posed.Handle, FVector(0, 200, 55)), 30.0, 0.05));
	TestTrue(TEXT("body stays put"), FMath::IsNearlyEqual(PieceMinZ(Posed.Handle, FVector(0, 0, 50)), 0.0, 0.05));
	const FModelingResult Refused = UModelingService::SaveMeshToSkeletalMeshLOD(Posed.Handle);
	TestTrue(FString::Printf(TEXT("posed handle cannot be saved as a reference pose: %s"), *Refused.Message), !Refused.bSuccess && Refused.Message.Contains(TEXT("POSED")));
	TestTrue(TEXT("posed handle cannot be saved as a NEW skeletal asset either"),
		!UModelingService::SaveMeshToSkeletalMesh(Posed.Handle, FString(kModelingTestDir) / TEXT("SK_PosedCopy"), TEXT(""), true, false).bSuccess);

	const FModelingResult InWorld = UModelingService::LoadMeshFromSkinnedComponent(TEXT("Vibe3DPoseTest"), TEXT("Pose"), true, 0, true);
	TestTrue(TEXT("world space adds the actor transform"), FMath::IsNearlyEqual(Bounds(InWorld.Handle).Min.X, 950.0, 0.05));
	const FModelingResult FromFollower = UModelingService::LoadMeshFromSkinnedComponent(TEXT("Vibe3DPoseTest"), TEXT("Follower"), false, 0, true);
	TestTrue(FString::Printf(TEXT("follower read: %s"), *FromFollower.Message), FromFollower.bSuccess && FromFollower.Message.Contains(TEXT("leader")));
	TestTrue(TEXT("follower takes the leader's pose"), FMath::IsNearlyEqual(PieceMinZ(FromFollower.Handle, FVector(0, 200, 55)), 30.0, 0.05));

	// Edit in the pose, unpose, write: the edit lands in the reference pose.
	UGeometryScriptLibrary_MeshTransformFunctions::TranslateMeshSelection(UModelingService::GetDynamicMesh(Posed.Handle), SelectPiece(Posed.Handle, TEXT("flap"), FVector(0, 200, 55)), FVector(0, 0, 5));
	const FModelingResult Unposed = UModelingService::UnposeMesh(Posed.Handle);
	TestTrue(FString::Printf(TEXT("unpose: %s"), *Unposed.Message), Unposed.bSuccess);
	TestTrue(TEXT("flap back in the reference pose, keeping the 5 cm edit"), FMath::IsNearlyEqual(PieceMinZ(Posed.Handle, FVector(0, 200, 25)), 5.0, 0.05));
	TestTrue(TEXT("body back where it was"), FMath::IsNearlyEqual(PieceMinZ(Posed.Handle, FVector(0, 0, 50)), 0.0, 0.05));
	const FModelingResult Written = UModelingService::SaveMeshToSkeletalMeshLOD(Posed.Handle, TEXT(""), -1, false);
	TestTrue(FString::Printf(TEXT("unposed handle writes back: %s"), *Written.Message), Written.bSuccess);
	TestTrue(TEXT("asset holds the edit"), FMath::IsNearlyEqual(PieceMinZ(UModelingService::LoadMeshFromSkeletalMesh(MeshPath, 0).Handle, FVector(0, 200, 25)), 5.0, 0.05));

	TestFalse(TEXT("unknown actor rejected"), UModelingService::LoadMeshFromSkinnedComponent(TEXT("NoSuchActor")).bSuccess);
	const FModelingResult NoComponent = UModelingService::LoadMeshFromSkinnedComponent(TEXT("Vibe3DPoseTest"), TEXT("Hat"));
	TestTrue(TEXT("unknown component lists the real ones"), !NoComponent.bSuccess && NoComponent.Message.Contains(TEXT("Follower")));
	TestFalse(TEXT("unpose of an unposed handle rejected"), UModelingService::UnposeMesh(Posed.Handle).bSuccess);

	Actor->Destroy();
	FAssetCompilingManager::Get().FinishAllCompilation();
	UModelingService::ReleaseAllMeshes();
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibe3DModelingGarmentFitTest, "Vibe3D.Modeling.GarmentFit", kModelingTestFlags)
bool FVibe3DModelingGarmentFitTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const auto Sphere = [](float Radius, bool bOpenTop)
	{
		const int32 H = MakeSphere(Radius, 24, 32);
		FGeometryScriptMeshPlaneCutOptions Cut;
		Cut.bFillHoles = false;
		if (bOpenTop) { UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(UModelingService::GetDynamicMesh(H), FTransform(FVector(0, 0, 30)), Cut); }   // a "shirt": no cover above z = 30
		return H;
	};
	const auto TopZ = [](int32 Handle)
	{
		double Top = -1e9;
		for (const FVector& P : UModelingService::GetVertexPositions(Handle)) { Top = FMath::Max(Top, P.Z); }
		return Top;
	};

	// Skin (r 50) pokes through a shirt (r 49, open above z 30).
	const int32 Body = Sphere(50.f, false);
	const int32 Shirt = Sphere(49.f, true);
	const FModelingPenetrationReport Before = UModelingService::MeasurePenetration(Body, Shirt, TEXT("Inside"), 0.3f, 5.f, TEXT(""), TEXT("poking"));
	TestTrue(FString::Printf(TEXT("penetration measured: %s"), *Before.Message), Before.bSuccess && Before.Violations > 0 && Before.MaxDepth > 1.0f && Before.MaxDepth < 3.0f);
	AddInfo(Before.Message);
	const double HeadTop = TopZ(Body);
	const FModelingResult Pushed = UModelingService::PushInside(Body, Shirt, 0.3f, TEXT(""), 5.f, 2);
	TestTrue(FString::Printf(TEXT("push inside: %s"), *Pushed.Message), Pushed.bSuccess);
	AddInfo(Pushed.Message);
	const FModelingPenetrationReport After = UModelingService::MeasurePenetration(Body, Shirt, TEXT("Inside"), 0.3f);
	TestTrue(FString::Printf(TEXT("skin under the shirt now: %s"), *After.Message), After.bSuccess && After.Violations == 0);
	TestTrue(TEXT("uncovered head left alone"), FMath::IsNearlyEqual(TopZ(Body), HeadTop, 1e-4));

	// A garment (r 48) sunk into the skin (r 50) is pushed out over it.
	const int32 Skin = Sphere(50.f, false);
	const int32 Sunk = Sphere(48.f, true);
	TestTrue(TEXT("sunk garment detected"), UModelingService::MeasurePenetration(Sunk, Skin, TEXT("Outside"), 0.3f).Violations > 0);
	TestTrue(TEXT("push outside"), UModelingService::PushOutside(Sunk, Skin, 0.3f, TEXT(""), 5.f, 2).bSuccess);
	const FModelingPenetrationReport Over = UModelingService::MeasurePenetration(Sunk, Skin, TEXT("Outside"), 0.3f);
	TestTrue(FString::Printf(TEXT("garment over the skin now: %s"), *Over.Message), Over.Violations == 0);

	// Skin well inside a garment can be dropped; the uncovered part stays.
	const int32 Inner = Sphere(45.f, false);
	const int32 Total = Tris(Inner);
	const int32 Hidden = UModelingService::SelectHiddenBy(Inner, TEXT("hidden"), Shirt, 0.5f, 10.f, true);
	TestTrue(FString::Printf(TEXT("hidden triangles found (%d of %d)"), Hidden, Total), Hidden > Total / 4 && Hidden < Total);

	TestFalse(TEXT("unknown side rejected"), UModelingService::MeasurePenetration(Body, Shirt, TEXT("Above")).bSuccess);
	TestFalse(TEXT("self fit rejected"), UModelingService::PushInside(Body, Body).bSuccess);
	TestEqual(TEXT("hidden-by on a missing handle"), UModelingService::SelectHiddenBy(Body, TEXT("x"), 4242), -1);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
