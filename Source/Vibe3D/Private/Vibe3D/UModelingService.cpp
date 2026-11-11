// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Vibe3D/UModelingService.h"

#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshRemeshFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshPolygroupFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshVertexColorFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBakeFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "GeometryScript/MeshPoolFunctions.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/OpenSubdivUtilityFunctions.h"
#include "GeometryScript/MeshSpatialFunctions.h"
#include "GeometryScript/MeshSamplingFunctions.h"
#include "GeometryScript/ContainmentFunctions.h"
#include "GeometryScript/MeshComparisonFunctions.h"
#include "GeometryScript/MeshSelectionQueryFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "Components/MeshComponent.h"

#include "Animation/Skeleton.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Misc/CoreDelegates.h"
#include "StaticMeshEditorSubsystem.h"
#include "StaticMeshEditorSubsystemHelpers.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Math/RandomStream.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Selections/MeshConnectedComponents.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "SkeletalMeshAttributes.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibe3DModeling, Log, All);

using namespace UE::Geometry;

// =========================================================================
// Session: integer handles -> UDynamicMesh (kept alive for the editor session)
// =========================================================================

namespace
{
	/** A named selection plus the mesh counts it was made against, so a stale one can be refused. */
	struct FStoredSelection
	{
		FGeometryScriptMeshSelection Selection;
		int32 TriangleCount = 0;
		int32 VertexCount = 0;
	};

	struct FModelingSession
	{
		TMap<int32, TStrongObjectPtr<UDynamicMesh>> Meshes;
		TMap<int32, TMap<FString, FStoredSelection>> Selections;
		int32 NextHandle = 1;
		bool bExitHooked = false;

		void Reset()
		{
			Selections.Reset();
			Meshes.Reset();
		}
	};

	FModelingSession& Session()
	{
		static FModelingSession S;
		if (!S.bExitHooked)
		{
			// Release the strong references before the UObject system tears down.
			S.bExitHooked = true;
			FCoreDelegates::OnPreExit.AddLambda([]() { Session().Reset(); });
		}
		return S;
	}

	UDynamicMesh* FindMesh(int32 Handle)
	{
		const TStrongObjectPtr<UDynamicMesh>* Found = Session().Meshes.Find(Handle);
		return Found ? Found->Get() : nullptr;
	}

	int32 AddMesh(UDynamicMesh* Mesh)
	{
		const int32 Handle = Session().NextHandle++;
		Session().Meshes.Add(Handle, TStrongObjectPtr<UDynamicMesh>(Mesh));
		return Handle;
	}

	UDynamicMesh* NewSessionMesh(int32& OutHandle)
	{
		UDynamicMesh* Mesh = NewObject<UDynamicMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		OutHandle = AddMesh(Mesh);
		return Mesh;
	}

	UDynamicMesh* NewScratchMesh()
	{
		return NewObject<UDynamicMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	int32 TriCount(UDynamicMesh* Mesh)
	{
		return Mesh ? Mesh->GetTriangleCount() : 0;
	}

	int32 VertCount(UDynamicMesh* Mesh)
	{
		int32 Count = 0;
		if (Mesh)
		{
			Mesh->ProcessMesh([&Count](const FDynamicMesh3& M) { Count = M.VertexCount(); });
		}
		return Count;
	}

	// ---- results -----------------------------------------------------------

	FModelingResult Fail(int32 Handle, const FString& Message)
	{
		FModelingResult R;
		R.bSuccess = false;
		R.Handle = Handle;
		R.Message = Message;
		UE_LOG(LogVibe3DModeling, Warning, TEXT("ModelingService failure (handle %d): %s"), Handle, *Message);
		return R;
	}

	FModelingResult Ok(int32 Handle, UDynamicMesh* Mesh, const FString& Message)
	{
		FModelingResult R;
		R.bSuccess = true;
		R.Handle = Handle;
		R.Message = Message;
		R.TriangleCount = TriCount(Mesh);
		R.VertexCount = VertCount(Mesh);
		return R;
	}

	FModelingResult NoMesh(int32 Handle)
	{
		return Fail(Handle, FString::Printf(TEXT("No session mesh with handle %d (create_mesh / load_mesh_* first, or see list_meshes)"), Handle));
	}

	// ---- GeometryScript debug capture --------------------------------------

	UGeometryScriptDebug* NewDebug()
	{
		return NewObject<UGeometryScriptDebug>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	bool DebugHasErrors(const UGeometryScriptDebug* Debug)
	{
		if (!Debug) { return false; }
		for (const FGeometryScriptDebugMessage& M : Debug->Messages)
		{
			if (M.MessageType == EGeometryScriptDebugMessageType::ErrorMessage || M.Message.ToString().Contains(TEXT("cannot be run"))) { return true; }
		}
		return false;
	}

	FString DebugText(const UGeometryScriptDebug* Debug)
	{
		TArray<FString> Lines;
		if (Debug)
		{
			for (const FGeometryScriptDebugMessage& M : Debug->Messages)
			{
				Lines.Add(M.Message.ToString());
			}
		}
		return FString::Join(Lines, TEXT("; "));
	}

	/** Standard end of a mutating call: errors reported by GeometryScript become a failure. */
	FModelingResult Finish(int32 Handle, UDynamicMesh* Mesh, UGeometryScriptDebug* Debug, const FString& What)
	{
		if (DebugHasErrors(Debug))
		{
			return Fail(Handle, What + TEXT(": ") + DebugText(Debug));
		}
		const FString Extra = DebugText(Debug);
		return Ok(Handle, Mesh, Extra.IsEmpty() ? What : What + TEXT(" (") + Extra + TEXT(")"));
	}

	// ---- enums by name -------------------------------------------------------

	template <typename TEnum>
	FString EnumNames()
	{
		TArray<FString> Names;
		const UEnum* E = StaticEnum<TEnum>();
		for (int32 i = 0; i < E->NumEnums() - 1; ++i)
		{
			Names.Add(E->GetNameStringByIndex(i));
		}
		return FString::Join(Names, TEXT(", "));
	}

	template <typename TEnum>
	bool ParseEnum(const FString& Name, TEnum& Out)
	{
		const UEnum* E = StaticEnum<TEnum>();
		const int64 Value = E->GetValueByNameString(Name.TrimStartAndEnd());
		if (Value == INDEX_NONE)
		{
			return false;
		}
		Out = static_cast<TEnum>(Value);
		return true;
	}

	// ---- selections ----------------------------------------------------------

	void StoreSelection(int32 Handle, const FString& Name, const FGeometryScriptMeshSelection& Selection)
	{
		FStoredSelection Stored;
		Stored.Selection = Selection;
		Stored.TriangleCount = TriCount(FindMesh(Handle));
		Stored.VertexCount = VertCount(FindMesh(Handle));
		Session().Selections.FindOrAdd(Handle).Add(Name, Stored);
	}

	/** Every element id the selection references must still exist — GeometryScript asserts on stale ids instead of failing. */
	bool SelectionIsValidForMesh(UDynamicMesh* Mesh, const FGeometryScriptMeshSelection& Selection)
	{
		FGeometryScriptIndexList IndexList;
		EGeometryScriptIndexType ResultType = EGeometryScriptIndexType::Any;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexList(Mesh, Selection, IndexList, ResultType, EGeometryScriptIndexType::Any);
		if (!IndexList.List.IsValid() || ResultType == EGeometryScriptIndexType::PolygroupID || ResultType == EGeometryScriptIndexType::Any)
		{
			return true;
		}
		bool bValid = true;
		Mesh->ProcessMesh([&](const FDynamicMesh3& M)
		{
			for (const int32 Id : *IndexList.List)
			{
				const bool bExists =
					ResultType == EGeometryScriptIndexType::Triangle ? M.IsTriangle(Id) :
					ResultType == EGeometryScriptIndexType::Vertex ? M.IsVertex(Id) :
					ResultType == EGeometryScriptIndexType::Edge ? M.IsEdge(Id) : true;
				if (!bExists) { bValid = false; return; }
			}
		});
		return bValid;
	}

	/** "" resolves to the whole mesh as a triangle selection; a named selection must predate no topology change. */
	bool ResolveSelection(int32 Handle, const FString& Name, UDynamicMesh* Mesh, FGeometryScriptMeshSelection& Out, FString& Error)
	{
		if (Name.IsEmpty())
		{
			UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(Mesh, Out, EGeometryScriptMeshSelectionType::Triangles);
			return true;
		}
		const TMap<FString, FStoredSelection>* PerMesh = Session().Selections.Find(Handle);
		const FStoredSelection* Found = PerMesh ? PerMesh->Find(Name) : nullptr;
		if (!Found)
		{
			Error = FString::Printf(TEXT("No selection named '%s' on handle %d (build one natively with unreal.GeometryScript_MeshSelection and store it with set_selection, or use select_by_bone_weight)"), *Name, Handle);
			return false;
		}
		const int32 Tris = TriCount(Mesh);
		const int32 Verts = VertCount(Mesh);
		if (Tris != Found->TriangleCount || Verts != Found->VertexCount || !SelectionIsValidForMesh(Mesh, Found->Selection))
		{
			Error = FString::Printf(TEXT("Selection '%s' is stale: handle %d changed since it was made (%d -> %d triangles). Re-run the select_* call after any op that changes topology."),
				*Name, Handle, Found->TriangleCount, Tris);
			return false;
		}
		Out = Found->Selection;
		return true;
	}

	/** Extrude, offset and inset keep the edited region's triangle ids, so its named selection follows those faces into the next op. */
	FModelingResult KeepRegionSelection(const FModelingResult& Result, int32 Handle, const FString& Name, UDynamicMesh* Mesh, const FGeometryScriptMeshSelection& Selection)
	{
		if (Result.bSuccess && !Name.IsEmpty() && Selection.GetSelectionType() == EGeometryScriptMeshSelectionType::Triangles && SelectionIsValidForMesh(Mesh, Selection))
		{
			StoreSelection(Handle, Name, Selection);
		}
		return Result;
	}

	FGeometryScriptPrimitiveOptions PrimitiveOptions(int32 MaterialID)
	{
		FGeometryScriptPrimitiveOptions Options;
		Options.MaterialID = MaterialID;
		return Options;
	}

	/** Triangle-id watermark taken before an append; pass to StampMaterialIDOnNewTriangles. */
	int32 TriangleIDWatermark(UDynamicMesh* Mesh)
	{
		int32 MaxTid = 0;
		if (Mesh)
		{
			Mesh->ProcessMesh([&MaxTid](const FDynamicMesh3& M) { MaxTid = M.MaxTriangleID(); });
		}
		return MaxTid;
	}

	/**
	 * Some engine primitive generators — AppendTorus, AppendRevolvePolygon and AppendLinearStairs —
	 * ignore FGeometryScriptPrimitiveOptions::MaterialID, so a caller's material_id silently landed
	 * as 0 and the part could not be given its own material slot. Stamp the id onto the triangles
	 * the append just created (everything at or above the pre-call watermark).
	 */
	void StampMaterialIDOnNewTriangles(UDynamicMesh* Mesh, int32 FirstNewTriangleID, int32 MaterialID)
	{
		if (!Mesh || MaterialID == 0)
		{
			return;   // 0 is what the generators already produce
		}
		Mesh->EditMesh([FirstNewTriangleID, MaterialID](FDynamicMesh3& M)
		{
			if (!M.HasAttributes())
			{
				M.EnableAttributes();
			}
			if (!M.Attributes()->HasMaterialID())
			{
				M.Attributes()->EnableMaterialID();
			}
			FDynamicMeshMaterialAttribute* Materials = M.Attributes()->GetMaterialID();
			for (const int32 Tid : M.TriangleIndicesItr())
			{
				if (Tid >= FirstNewTriangleID)
				{
					Materials->SetValue(Tid, MaterialID);
				}
			}
		}, EDynamicMeshChangeType::AttributeEdit);   // no MaterialIDs flag exists; Unknown is the default hint
	}

	bool ParseOrigin(const FString& Origin, EGeometryScriptPrimitiveOriginMode& Out, FString& Error)
	{
		if (ParseEnum(Origin, Out)) { return true; }
		Error = FString::Printf(TEXT("Unknown origin '%s' (use %s)"), *Origin, *EnumNames<EGeometryScriptPrimitiveOriginMode>());
		return false;
	}

	// ---- assets ----------------------------------------------------------------

	template <typename T>
	T* LoadAssetAs(const FString& AssetPath)
	{
		// DoesAssetExist first: LoadAsset logs an error for a missing path, which agents (and automation) read as a failure of their own.
		if (AssetPath.IsEmpty() || !UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			return nullptr;
		}
		return Cast<T>(UEditorAssetLibrary::LoadAsset(AssetPath));
	}

	bool CopyFromStaticMesh(UStaticMesh* StaticMesh, UDynamicMesh* Target, int32 LODIndex, UGeometryScriptDebug* Debug)
	{
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
		FGeometryScriptMeshReadLOD ReadLOD;
		ReadLOD.LODIndex = LODIndex;
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(StaticMesh, Target, AssetOptions, ReadLOD, Outcome, Debug);
		return Outcome == EGeometryScriptOutcomePins::Success;
	}

	bool CopyFromSkeletalMesh(USkeletalMesh* SkeletalMesh, UDynamicMesh* Target, int32 LODIndex, UGeometryScriptDebug* Debug)
	{
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
		FGeometryScriptMeshReadLOD ReadLOD;
		ReadLOD.LODIndex = LODIndex;
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(SkeletalMesh, Target, AssetOptions, ReadLOD, Outcome, Debug);
		return Outcome == EGeometryScriptOutcomePins::Success;
	}

	void SaveIf(bool bSave, const FString& AssetPath)
	{
		if (bSave)
		{
			UEditorAssetLibrary::SaveAsset(AssetPath, false);
		}
	}

	AActor* FindActorByLabel(const FString& Label)
	{
		UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
		if (!ActorSubsystem) { return nullptr; }
		for (AActor* Actor : ActorSubsystem->GetAllLevelActors())
		{
			if (Actor && Actor->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase))
			{
				return Actor;
			}
		}
		return nullptr;
	}
}

// =========================================================================
// Session
// =========================================================================

FModelingResult UModelingService::CreateMesh()
{
	int32 Handle = -1;
	UDynamicMesh* Mesh = NewSessionMesh(Handle);
	return Ok(Handle, Mesh, TEXT("Created empty mesh"));
}

bool UModelingService::ReleaseMesh(int32 Handle)
{
	Session().Selections.Remove(Handle);
	return Session().Meshes.Remove(Handle) > 0;
}

int32 UModelingService::ReleaseAllMeshes()
{
	const int32 Count = Session().Meshes.Num();
	Session().Reset();
	return Count;
}

TArray<int32> UModelingService::ListMeshes()
{
	TArray<int32> Handles;
	Session().Meshes.GetKeys(Handles);
	Handles.Sort();
	return Handles;
}

UDynamicMesh* UModelingService::GetDynamicMesh(int32 Handle)
{
	return FindMesh(Handle);
}

FModelingResult UModelingService::AppendTorus(int32 Handle, FTransform Transform, float MajorRadius, float MinorRadius, int32 MajorSteps,
	int32 MinorSteps, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptRevolveOptions RevolveOptions;
	const int32 Watermark = TriangleIDWatermark(Mesh);
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(Mesh, PrimitiveOptions(MaterialID), Transform, RevolveOptions, MajorRadius, MinorRadius,
		MajorSteps, MinorSteps, OriginMode, Debug);
	StampMaterialIDOnNewTriangles(Mesh, Watermark, MaterialID);   // the generator drops PrimitiveOptions.MaterialID
	return Finish(Handle, Mesh, Debug, TEXT("Appended torus"));
}

FModelingResult UModelingService::AppendRevolvePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, float Radius,
	int32 Steps, float RevolveDegrees, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (ProfilePoints.Num() < 3) { return Fail(Handle, TEXT("ProfilePoints needs at least 3 points")); }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptRevolveOptions RevolveOptions;
	RevolveOptions.RevolveDegrees = RevolveDegrees;
	const int32 Watermark = TriangleIDWatermark(Mesh);
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePolygon(Mesh, PrimitiveOptions(MaterialID), Transform, ProfilePoints, RevolveOptions, Radius, Steps, Debug);
	StampMaterialIDOnNewTriangles(Mesh, Watermark, MaterialID);   // the generator drops PrimitiveOptions.MaterialID
	return Finish(Handle, Mesh, Debug, TEXT("Appended revolved profile"));
}

int32 UModelingService::SetSelection(int32 Handle, const FString& SelectionName, const FGeometryScriptMeshSelection& Selection)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty() || !SelectionIsValidForMesh(Mesh, Selection)) { return -1; }
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

bool UModelingService::ClearSelections(int32 Handle)
{
	return Session().Selections.Remove(Handle) > 0;
}

FModelingResult UModelingService::SaveMeshToSkeletalMesh(int32 Handle, const FString& AssetPath, const FString& SkeletonPath, bool bReplaceExisting, bool bSaveAsset)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (TriCount(Mesh) == 0) { return Fail(Handle, TEXT("Mesh is empty; nothing to save")); }
	if (!AssetPath.StartsWith(TEXT("/"))) { return Fail(Handle, FString::Printf(TEXT("AssetPath must be a full package path like /Game/Folder/SK_Name (got '%s')"), *AssetPath)); }

	bool bMeshHasBones = false;
	bool bMeshHasWeights = false;
	int32 MaxMaterialID = 0;
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		if (!M.HasAttributes()) { return; }
		bMeshHasBones = M.Attributes()->HasBones();
		bMeshHasWeights = M.Attributes()->GetSkinWeightsAttributes().Num() > 0;
		if (M.Attributes()->HasMaterialID())
		{
			const FDynamicMeshMaterialAttribute* MaterialIDs = M.Attributes()->GetMaterialID();
			for (const int32 Tid : M.TriangleIndicesItr()) { MaxMaterialID = FMath::Max(MaxMaterialID, MaterialIDs->GetValue(Tid)); }
		}
	});
	if (!bMeshHasWeights)
	{
		return Fail(Handle, TEXT("Mesh has no skin weights: run create_bones + bind_selection_to_bone (or native GeometryScript_BoneWeights.transfer_bone_weights_from_mesh) first"));
	}

	const bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
	USkeleton* Skeleton = nullptr;
	// Only a skeleton this call created (or its own <AssetPath>_Skeleton) is ours to modify and save. A skeleton passed in by path is
	// usually shared (an engine MetaHuman skeleton, a game skeleton): binding a new mesh to it must never re-save it.
	bool bOwnSkeleton = false;
	FString SkeletonNote;
	if (!SkeletonPath.IsEmpty())
	{
		Skeleton = LoadAssetAs<USkeleton>(SkeletonPath);
		if (!Skeleton) { return Fail(Handle, FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath)); }
	}
	else if (!bExists)
	{
		if (!bMeshHasBones)
		{
			return Fail(Handle, TEXT("No SkeletonPath and the mesh has no bones: pass an existing skeleton, or define bones with create_bones so one can be created"));
		}
		// Create an empty Skeleton next to the mesh; CreateSkeletalMeshAsset merges the mesh's bones into it.
		const FString NewSkeletonPath = AssetPath + TEXT("_Skeleton");
		if (UEditorAssetLibrary::DoesAssetExist(NewSkeletonPath))
		{
			Skeleton = LoadAssetAs<USkeleton>(NewSkeletonPath);
			if (!Skeleton) { return Fail(Handle, FString::Printf(TEXT("%s exists but is not a Skeleton"), *NewSkeletonPath)); }
		}
		else
		{
			UPackage* Package = CreatePackage(*NewSkeletonPath);
			Skeleton = NewObject<USkeleton>(Package, *FPackageName::GetLongPackageAssetName(NewSkeletonPath), RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Skeleton);
			Package->MarkPackageDirty();
		}
		bOwnSkeleton = true;
		SkeletonNote = FString::Printf(TEXT(" with skeleton %s"), *NewSkeletonPath);
	}

	UGeometryScriptDebug* Debug = NewDebug();
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	USkeletalMesh* SkeletalMesh = nullptr;
	FString What;
	if (bExists)
	{
		SkeletalMesh = LoadAssetAs<USkeletalMesh>(AssetPath);
		if (!SkeletalMesh) { return Fail(Handle, FString::Printf(TEXT("%s exists but is not a SkeletalMesh"), *AssetPath)); }
		if (!bReplaceExisting) { return Fail(Handle, FString::Printf(TEXT("%s already exists (pass replace_existing=True to overwrite LOD0)"), *AssetPath)); }
		FGeometryScriptCopyMeshToAssetOptions Options;
		FGeometryScriptMeshWriteLOD WriteLOD;
		WriteLOD.LODIndex = 0;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(Mesh, SkeletalMesh, Options, WriteLOD, Outcome, Debug);
		What = FString::Printf(TEXT("Replaced LOD0 of %s"), *AssetPath);
	}
	else
	{
		FGeometryScriptCreateNewSkeletalMeshAssetOptions Options;
		// Bones authored on the mesh (create_bones, or copied from a source skeletal mesh) define the reference skeleton.
		Options.bUseMeshBoneProportions = bMeshHasBones;
		// One slot per material ID so multi-material meshes keep their sections; set_asset_materials fills them in.
		for (int32 i = 0; i <= MaxMaterialID; ++i)
		{
			Options.Materials.Add(*FString::Printf(TEXT("Slot%d"), i), UMaterial::GetDefaultMaterial(MD_Surface));
		}
		SkeletalMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMesh(Mesh, Skeleton, AssetPath, Options, Outcome, Debug);
		if (SkeletalMesh && Skeleton && bOwnSkeleton && !Skeleton->GetPreviewMesh())
		{
			Skeleton->SetPreviewMesh(SkeletalMesh);
		}
		What = FString::Printf(TEXT("Created %s%s (%d material slot(s))"), *AssetPath, *SkeletonNote, MaxMaterialID + 1);
	}
	if (Outcome != EGeometryScriptOutcomePins::Success || !SkeletalMesh)
	{
		return Fail(Handle, FString::Printf(TEXT("Saving to %s failed: %s"), *AssetPath, *DebugText(Debug)));
	}
	SaveIf(bSaveAsset, SkeletalMesh->GetPathName());
	if (Skeleton && bOwnSkeleton) { SaveIf(bSaveAsset, Skeleton->GetPathName()); }
	FModelingResult R = Ok(Handle, Mesh, What);
	R.AssetPath = SkeletalMesh->GetPathName();
	return R;
}

namespace
{
	/** Shared tail of every bake: run the baker, then save one Texture2D asset per requested type (T_<BaseName>_<Suffix>). */
	FModelingBakeResult RunBake(UDynamicMesh* Target, UDynamicMesh* Source, const TArray<FGeometryScriptBakeTypeOptions>& Types, const TArray<FString>& Suffixes,
		int32 Resolution, const FString& OutputFolder, const FString& BaseName, int32 TargetUVLayer, int32 SamplesPerPixel)
	{
		FModelingBakeResult Result;
		FGeometryScriptBakeTargetMeshOptions TargetOptions;
		TargetOptions.TargetUVLayer = TargetUVLayer;
		FGeometryScriptBakeSourceMeshOptions SourceOptions;
		FGeometryScriptBakeTextureOptions BakeOptions;
		if (!ParseEnum(FString::Printf(TEXT("Resolution%d"), Resolution), BakeOptions.Resolution))
		{
			Result.Message = FString::Printf(TEXT("Unsupported resolution %d (use a power of two from 16 to 8192)"), Resolution);
			return Result;
		}
		if (!ParseEnum(FString::Printf(TEXT("Sample%d"), SamplesPerPixel), BakeOptions.SamplesPerPixel))
		{
			BakeOptions.SamplesPerPixel = EGeometryScriptBakeSamplesPerPixel::Sample1;
		}
		UGeometryScriptDebug* Debug = NewDebug();
		// The baker samples through the target's tangent frame; session meshes rarely carry tangents yet.
		UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(Target, FGeometryScriptTangentsOptions(), Debug);
		const TArray<UTexture2D*> Textures = UGeometryScriptLibrary_MeshBakeFunctions::BakeTexture(Target, FTransform::Identity, TargetOptions,
			Source, FTransform::Identity, SourceOptions, Types, BakeOptions, Debug);
		if (DebugHasErrors(Debug) || Textures.Num() != Types.Num())
		{
			Result.Message = FString::Printf(TEXT("Bake failed: %s"), *DebugText(Debug));
			return Result;
		}
		for (int32 i = 0; i < Textures.Num(); ++i)
		{
			if (!Textures[i]) { continue; }
			FGeometryScriptCreateNewTexture2DAssetOptions TextureOptions;
			TextureOptions.bOverwriteIfExists = true;
			EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
			const FString AssetPath = OutputFolder / FString::Printf(TEXT("T_%s_%s"), *BaseName, *Suffixes[i]);
			UTexture2D* Saved = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewTexture2DAsset(Textures[i], AssetPath, TextureOptions, Outcome, Debug);
			if (Outcome != EGeometryScriptOutcomePins::Success || !Saved)
			{
				Result.Message = FString::Printf(TEXT("Could not save %s: %s"), *AssetPath, *DebugText(Debug));
				return Result;
			}
			Saved->BlockOnAnyAsyncBuild();   // the texture compiles asynchronously; finish it so a save or delete right after cannot race it
			UEditorAssetLibrary::SaveAsset(Saved->GetPathName(), false);
			Result.TexturePaths.Add(Saved->GetPathName());
		}
		Result.bSuccess = true;
		Result.Message = FString::Printf(TEXT("Baked %d texture(s) at %d"), Result.TexturePaths.Num(), Resolution);
		return Result;
	}
}

// =========================================================================
// Hulls and comparison
// =========================================================================

namespace
{
	/**
	 * Booleans, deletes, and cuts leave gaps in the vertex/triangle id space. The containment
	 * algorithms index by id and assert on a non-compact mesh, so hull operations run on a
	 * compacted scratch copy of the source (the session mesh itself is left untouched).
	 */
	UDynamicMesh* CompactedCopy(UDynamicMesh* Source, UGeometryScriptDebug* Debug)
	{
		UDynamicMesh* Scratch = NewScratchMesh();
		UDynamicMesh* Out = nullptr;
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(Source, Scratch, Out, Debug);
		UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Scratch, Debug);
		return Scratch;
	}
}

// =========================================================================
// Rigging: bones and skin weights authored on the mesh
// =========================================================================

namespace
{
	using FSkinWeights = FDynamicMeshVertexSkinWeightsAttribute;
	using UE::AnimationCore::FBoneWeight;
	using UE::AnimationCore::FBoneWeights;

	/** Mesh-space reference pose of every bone (poses are stored relative to the parent, parents precede children). */
	TArray<FTransform> BoneMeshTransforms(const FDynamicMeshAttributeSet* Attributes)
	{
		TArray<FTransform> Out;
		const int32 Num = Attributes->GetNumBones();
		Out.SetNum(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			const int32 Parent = Attributes->GetBoneParentIndices()->GetValue(i);
			const FTransform& Local = Attributes->GetBonePoses()->GetValue(i);
			Out[i] = (Parent >= 0 && Parent < i) ? Local * Out[Parent] : Local;
		}
		return Out;
	}

	int32 FindBoneIndexByName(const FDynamicMeshAttributeSet* Attributes, const FName Name)
	{
		for (int32 i = 0; i < Attributes->GetNumBones(); ++i)
		{
			if (Attributes->GetBoneNames()->GetValue(i) == Name) { return i; }
		}
		return INDEX_NONE;
	}

	FSkinWeights* DefaultSkinWeights(FDynamicMesh3& M)
	{
		return M.HasAttributes() ? M.Attributes()->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName) : nullptr;
	}
}

FModelingResult UModelingService::CreateBones(int32 Handle, const TArray<FModelingBoneDef>& Bones)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (Bones.Num() == 0) { return Fail(Handle, TEXT("Bones is empty: pass at least a root bone")); }

	TMap<FName, int32> IndexByName;
	TArray<FName> Names;
	TArray<int32> ParentIndices;
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const FName Name(*Bones[i].Name.TrimStartAndEnd());
		if (Name.IsNone()) { return Fail(Handle, FString::Printf(TEXT("Bone %d has no name"), i)); }
		if (IndexByName.Contains(Name)) { return Fail(Handle, FString::Printf(TEXT("Duplicate bone name '%s'"), *Name.ToString())); }
		const FString ParentName = Bones[i].ParentName.TrimStartAndEnd();
		int32 Parent = INDEX_NONE;
		if (i == 0)
		{
			if (!ParentName.IsEmpty() && ParentName != TEXT("None"))
			{
				return Fail(Handle, FString::Printf(TEXT("The first bone ('%s') is the root and cannot have a parent"), *Name.ToString()));
			}
		}
		else
		{
			const int32* Found = ParentName.IsEmpty() ? nullptr : IndexByName.Find(FName(*ParentName));
			if (!Found)
			{
				return Fail(Handle, FString::Printf(TEXT("Bone '%s' needs a parent listed before it (got '%s'); only the first bone is a root"), *Name.ToString(), *ParentName));
			}
			Parent = *Found;
		}
		IndexByName.Add(Name, i);
		Names.Add(Name);
		ParentIndices.Add(Parent);
	}

	Mesh->EditMesh([&](FDynamicMesh3& M)
	{
		M.EnableAttributes();
		FDynamicMeshAttributeSet* Attributes = M.Attributes();
		Attributes->DisableBones();
		Attributes->EnableBones(Bones.Num());
		TArray<FTransform> MeshSpace;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const FTransform& World = Bones[i].Transform;
			MeshSpace.Add(World);
			const int32 Parent = ParentIndices[i];
			Attributes->GetBoneNames()->SetValue(i, Names[i]);
			Attributes->GetBoneParentIndices()->SetValue(i, Parent);
			Attributes->GetBonePoses()->SetValue(i, Parent >= 0 ? World.GetRelativeTransform(MeshSpace[Parent]) : World);
		}
	});

	// A fresh default profile with everything on the root, so the mesh is a valid skeletal mesh immediately.
	bool bProfileExisted = false;
	UGeometryScriptLibrary_MeshBoneWeightFunctions::MeshCreateBoneWeights(Mesh, bProfileExisted, true, FGeometryScriptBoneWeightProfile());
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBoneWeightFunctions::SetAllVertexBoneWeights(Mesh, TArray<FGeometryScriptBoneWeight>{ FGeometryScriptBoneWeight(0, 1.f) }, FGeometryScriptBoneWeightProfile(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Created %d bone(s) rooted at '%s'; every vertex bound to the root (bind_selection_to_bone to assign pieces)"), Bones.Num(), *Names[0].ToString()));
}

FModelingResult UModelingService::BindSelectionToBone(int32 Handle, const FString& SelectionName, const FString& BoneName, float Weight)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection;
	FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }

	int32 NumBones = 0;
	int32 BoneIndex = INDEX_NONE;
	const FName Bone(*BoneName.TrimStartAndEnd());
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		if (M.HasAttributes() && M.Attributes()->HasBones())
		{
			NumBones = M.Attributes()->GetNumBones();
			BoneIndex = FindBoneIndexByName(M.Attributes(), Bone);
		}
	});
	if (NumBones == 0) { return Fail(Handle, TEXT("Mesh has no bones: run create_bones first (or load a skeletal mesh)")); }
	if (BoneIndex == INDEX_NONE) { return Fail(Handle, FString::Printf(TEXT("No bone named '%s' (see native GeometryScript_BoneWeights.get_all_bones_info)"), *BoneName)); }

	bool bProfileExisted = false;
	UGeometryScriptLibrary_MeshBoneWeightFunctions::MeshCreateBoneWeights(Mesh, bProfileExisted, false, FGeometryScriptBoneWeightProfile());

	FGeometryScriptIndexList Vertices;
	EGeometryScriptIndexType ResultType = EGeometryScriptIndexType::Any;
	UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexList(Mesh, Selection, Vertices, ResultType, EGeometryScriptIndexType::Vertex);
	if (!Vertices.List.IsValid() || ResultType != EGeometryScriptIndexType::Vertex)
	{
		return Fail(Handle, FString::Printf(TEXT("Selection '%s' could not be converted to vertices"), *SelectionName));
	}

	const float W = FMath::Clamp(Weight, 0.f, 1.f);
	int32 Count = 0;
	Mesh->EditMesh([&](FDynamicMesh3& M)
	{
		FSkinWeights* Skin = DefaultSkinWeights(M);
		if (!Skin) { return; }
		for (const int32 Vid : *Vertices.List)
		{
			if (!M.IsVertex(Vid)) { continue; }
			TArray<FBoneWeight> NewWeights;
			if (W < 1.f)
			{
				FBoneWeights Existing;
				Skin->GetValue(Vid, Existing);
				for (int32 k = 0; k < Existing.Num(); ++k)
				{
					const FBoneWeight& Old = Existing[k];
					if (Old.GetBoneIndex() != BoneIndex && Old.GetWeight() > 0.f)
					{
						NewWeights.Add(FBoneWeight(Old.GetBoneIndex(), Old.GetWeight() * (1.f - W)));
					}
				}
			}
			NewWeights.Add(FBoneWeight(static_cast<FBoneIndexType>(BoneIndex), W));
			Skin->SetValue(Vid, FBoneWeights::Create(NewWeights));
			++Count;
		}
	});
	return Ok(Handle, Mesh, FString::Printf(TEXT("Bound %d vertices to '%s' (weight %.2f)"), Count, *Bone.ToString(), W));
}

namespace
{
	/** Comma-separated bone names -> attribute bone indices. Returns false and fills Error when any name is missing or the list is empty. */
	bool ParseBoneIndices(const FDynamicMeshAttributeSet* Attributes, const FString& BoneNames, TSet<int32>& OutIndices, FString& Error)
	{
		TArray<FString> Names;
		BoneNames.ParseIntoArray(Names, TEXT(","), true);
		for (FString& Name : Names)
		{
			Name.TrimStartAndEndInline();
			if (Name.IsEmpty()) { continue; }
			const int32 Index = FindBoneIndexByName(Attributes, FName(*Name));
			if (Index == INDEX_NONE)
			{
				Error = FString::Printf(TEXT("No bone named '%s' (see native GeometryScript_BoneWeights.get_all_bones_info)"), *Name);
				return false;
			}
			OutIndices.Add(Index);
		}
		if (OutIndices.Num() == 0)
		{
			Error = TEXT("BoneNames must list at least one bone (comma-separated)");
			return false;
		}
		return true;
	}

	/** Selection ("" = whole mesh) -> vertex id array. */
	bool SelectionVertices(int32 Handle, const FString& SelectionName, UDynamicMesh* Mesh, TArray<int32>& OutVertices, FString& Error)
	{
		FGeometryScriptMeshSelection Selection;
		if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return false; }
		FGeometryScriptIndexList Vertices;
		EGeometryScriptIndexType ResultType = EGeometryScriptIndexType::Any;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexList(Mesh, Selection, Vertices, ResultType, EGeometryScriptIndexType::Vertex);
		if (!Vertices.List.IsValid() || ResultType != EGeometryScriptIndexType::Vertex)
		{
			Error = FString::Printf(TEXT("Selection '%s' could not be converted to vertices"), *SelectionName);
			return false;
		}
		OutVertices = *Vertices.List;
		return true;
	}
}

int32 UModelingService::SelectByBoneWeight(int32 Handle, const FString& SelectionName, const FString& BoneNames, float MinWeight)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	const float Threshold = FMath::Clamp(MinWeight, 0.f, 1.f);
	TArray<int32> Vids;
	FString Error;
	bool bOk = false;
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		if (!M.HasAttributes() || !M.Attributes()->HasBones()) { Error = TEXT("Mesh has no bones: run create_bones or load a skeletal mesh first"); return; }
		TSet<int32> Targets;
		if (!ParseBoneIndices(M.Attributes(), BoneNames, Targets, Error)) { return; }
		const FSkinWeights* Skin = M.Attributes()->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName);
		if (!Skin) { Error = TEXT("Mesh has no skin weights: bind_selection_to_bone (or native transfer_bone_weights_from_mesh) first"); return; }
		FBoneWeights Weights;
		for (const int32 Vid : M.VertexIndicesItr())
		{
			Skin->GetValue(Vid, Weights);
			float Sum = 0.f;
			for (int32 k = 0; k < Weights.Num(); ++k)
			{
				if (Targets.Contains(static_cast<int32>(Weights[k].GetBoneIndex()))) { Sum += Weights[k].GetWeight(); }
			}
			if (Sum > 0.f && Sum >= Threshold) { Vids.Add(Vid); }
		}
		bOk = true;
	});
	if (!bOk)
	{
		UE_LOG(LogVibe3DModeling, Warning, TEXT("SelectByBoneWeight: %s"), *Error);
		return -1;
	}
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(Mesh, Vids, EGeometryScriptMeshSelectionType::Vertices, Selection);
	StoreSelection(Handle, SelectionName, Selection);
	return Vids.Num();
}

FModelingResult UModelingService::ScaleBoneWeights(int32 Handle, const FString& SelectionName, const FString& BoneNames, float Scale, const FString& ReceiverBoneName)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	TArray<int32> Vids;
	FString Error;
	if (!SelectionVertices(Handle, SelectionName, Mesh, Vids, Error)) { return Fail(Handle, Error); }

	const float S = FMath::Clamp(Scale, 0.f, 1.f);
	const FString Receiver = ReceiverBoneName.TrimStartAndEnd();
	int32 Count = 0;
	int32 SkippedFullyBound = 0;
	bool bOk = false;
	Mesh->EditMesh([&](FDynamicMesh3& M)
	{
		if (!M.HasAttributes() || !M.Attributes()->HasBones()) { Error = TEXT("Mesh has no bones: run create_bones or load a skeletal mesh first"); return; }
		TSet<int32> Targets;
		if (!ParseBoneIndices(M.Attributes(), BoneNames, Targets, Error)) { return; }
		int32 ReceiverIndex = INDEX_NONE;
		if (!Receiver.IsEmpty())
		{
			ReceiverIndex = FindBoneIndexByName(M.Attributes(), FName(*Receiver));
			if (ReceiverIndex == INDEX_NONE) { Error = FString::Printf(TEXT("No bone named '%s' (see native GeometryScript_BoneWeights.get_all_bones_info)"), *Receiver); return; }
			if (Targets.Contains(ReceiverIndex)) { Error = FString::Printf(TEXT("Receiver '%s' is also in BoneNames; the freed weight must go to a different bone"), *Receiver); return; }
		}
		FSkinWeights* Skin = DefaultSkinWeights(M);
		if (!Skin) { Error = TEXT("Mesh has no skin weights: bind_selection_to_bone (or native transfer_bone_weights_from_mesh) first"); return; }

		FBoneWeights Existing;
		for (const int32 Vid : Vids)
		{
			if (!M.IsVertex(Vid)) { continue; }
			Skin->GetValue(Vid, Existing);
			TMap<int32, float> W;
			float TargetSum = 0.f;
			for (int32 k = 0; k < Existing.Num(); ++k)
			{
				const int32 BoneIndex = static_cast<int32>(Existing[k].GetBoneIndex());
				const float Weight = Existing[k].GetWeight();
				if (Weight <= 0.f) { continue; }
				W.FindOrAdd(BoneIndex) += Weight;
				if (Targets.Contains(BoneIndex)) { TargetSum += Weight; }
			}
			if (TargetSum <= 0.f) { continue; }                 // vertex untouched by the listed bones

			const float Freed = TargetSum * (1.f - S);
			for (const int32 BoneIndex : Targets)
			{
				if (float* Found = W.Find(BoneIndex)) { *Found *= S; }
			}

			int32 Into = ReceiverIndex;
			if (Into == INDEX_NONE)
			{
				float Best = 0.f;                                // strongest influence outside the target set
				for (const TPair<int32, float>& Pair : W)
				{
					if (!Targets.Contains(Pair.Key) && Pair.Value > Best) { Best = Pair.Value; Into = Pair.Key; }
				}
			}
			if (Into == INDEX_NONE) { ++SkippedFullyBound; continue; }  // nothing else owns this vertex; renormalizing would undo the scale

			W.FindOrAdd(Into) += Freed;
			TArray<FBoneWeight> NewWeights;
			for (const TPair<int32, float>& Pair : W)
			{
				if (Pair.Value > 0.f) { NewWeights.Add(FBoneWeight(static_cast<FBoneIndexType>(Pair.Key), Pair.Value)); }
			}
			Skin->SetValue(Vid, FBoneWeights::Create(NewWeights));
			++Count;
		}
		bOk = true;
	});
	if (!bOk) { return Fail(Handle, Error); }
	FString Skipped = SkippedFullyBound > 0 ? FString::Printf(TEXT("; %d vertices bound only to those bones skipped (name a ReceiverBoneName to move them)"), SkippedFullyBound) : FString();
	return Ok(Handle, Mesh, FString::Printf(TEXT("Scaled [%s] to %.2f on %d vertices, freed weight -> %s%s"),
		*BoneNames, S, Count, Receiver.IsEmpty() ? TEXT("strongest other influence") : *Receiver, *Skipped));
}

FModelingResult UModelingService::SmoothSelectionBoneWeights(int32 Handle, const FString& SelectionName, int32 Iterations, float Alpha)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	TArray<int32> Vids;
	FString Error;
	if (!SelectionVertices(Handle, SelectionName, Mesh, Vids, Error)) { return Fail(Handle, Error); }
	const int32 Iters = FMath::Clamp(Iterations, 1, 50);
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);

	int32 Count = 0;
	bool bOk = false;
	Mesh->EditMesh([&](FDynamicMesh3& M)
	{
		FSkinWeights* Skin = DefaultSkinWeights(M);
		if (!Skin) { Error = TEXT("Mesh has no skin weights: bind_selection_to_bone (or native transfer_bone_weights_from_mesh) first"); return; }

		const auto ReadWeights = [Skin](int32 Vid, TMap<int32, float>& Out)
		{
			FBoneWeights Weights;
			Skin->GetValue(Vid, Weights);
			for (int32 k = 0; k < Weights.Num(); ++k)
			{
				if (Weights[k].GetWeight() > 0.f) { Out.FindOrAdd(static_cast<int32>(Weights[k].GetBoneIndex())) += Weights[k].GetWeight(); }
			}
		};

		for (int32 Iter = 0; Iter < Iters; ++Iter)
		{
			// Jacobi step: gather smoothed values from the current state, then apply, so edit order can't bias the result.
			TArray<TPair<int32, TMap<int32, float>>> Pending;
			Pending.Reserve(Vids.Num());
			for (const int32 Vid : Vids)
			{
				if (!M.IsVertex(Vid)) { continue; }
				TMap<int32, float> Avg;
				int32 NumNeighbours = 0;
				for (const int32 Nbr : M.VtxVerticesItr(Vid))
				{
					ReadWeights(Nbr, Avg);
					++NumNeighbours;
				}
				if (NumNeighbours == 0) { continue; }
				for (TPair<int32, float>& Pair : Avg) { Pair.Value /= static_cast<float>(NumNeighbours); }

				TMap<int32, float> Own;
				ReadWeights(Vid, Own);
				TMap<int32, float> Blended;
				for (const TPair<int32, float>& Pair : Own) { Blended.FindOrAdd(Pair.Key) += Pair.Value * (1.f - A); }
				for (const TPair<int32, float>& Pair : Avg) { Blended.FindOrAdd(Pair.Key) += Pair.Value * A; }
				Pending.Emplace(Vid, MoveTemp(Blended));
			}
			for (TPair<int32, TMap<int32, float>>& Entry : Pending)
			{
				TArray<FBoneWeight> NewWeights;
				for (const TPair<int32, float>& Pair : Entry.Value)
				{
					if (Pair.Value > 0.005f) { NewWeights.Add(FBoneWeight(static_cast<FBoneIndexType>(Pair.Key), Pair.Value)); }  // drop dust so influences don't creep
				}
				if (NewWeights.Num() > 0)
				{
					Skin->SetValue(Entry.Key, FBoneWeights::Create(NewWeights));
					if (Iter == 0) { ++Count; }
				}
			}
		}
		bOk = true;
	});
	if (!bOk) { return Fail(Handle, Error); }
	return Ok(Handle, Mesh, FString::Printf(TEXT("Smoothed skin weights on %d vertices (%d iteration(s), alpha %.2f)"), Count, Iters, A));
}

FModelingResult UModelingService::InspectBoneWeights(int32 Handle, const FString& SelectionName)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	TArray<int32> Vids;
	FString Error;
	if (!SelectionVertices(Handle, SelectionName, Mesh, Vids, Error)) { return Fail(Handle, Error); }

	struct FBoneStat { int32 Vertices = 0; float SumWeight = 0.f; float MaxWeight = 0.f; };
	TMap<int32, FBoneStat> Stats;
	float MinSum = TNumericLimits<float>::Max();
	float MaxSum = 0.f;
	int32 MaxInfluences = 0;
	int32 OffNormalized = 0;
	int32 Counted = 0;
	TArray<FName> BoneNames;
	bool bOk = false;
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		const FSkinWeights* Skin = M.HasAttributes() ? M.Attributes()->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName) : nullptr;
		if (!Skin) { Error = TEXT("Mesh has no skin weights: bind_selection_to_bone (or native transfer_bone_weights_from_mesh) first"); return; }
		if (M.Attributes()->HasBones())
		{
			for (int32 i = 0; i < M.Attributes()->GetNumBones(); ++i) { BoneNames.Add(M.Attributes()->GetBoneNames()->GetValue(i)); }
		}
		FBoneWeights Weights;
		for (const int32 Vid : Vids)
		{
			if (!M.IsVertex(Vid)) { continue; }
			Skin->GetValue(Vid, Weights);
			float Sum = 0.f;
			int32 Influences = 0;
			for (int32 k = 0; k < Weights.Num(); ++k)
			{
				const float Weight = Weights[k].GetWeight();
				if (Weight <= 0.f) { continue; }
				FBoneStat& Stat = Stats.FindOrAdd(static_cast<int32>(Weights[k].GetBoneIndex()));
				++Stat.Vertices;
				Stat.SumWeight += Weight;
				Stat.MaxWeight = FMath::Max(Stat.MaxWeight, Weight);
				Sum += Weight;
				++Influences;
			}
			MinSum = FMath::Min(MinSum, Sum);
			MaxSum = FMath::Max(MaxSum, Sum);
			MaxInfluences = FMath::Max(MaxInfluences, Influences);
			if (FMath::Abs(Sum - 1.f) > 0.01f) { ++OffNormalized; }
			++Counted;
		}
		bOk = true;
	});
	if (!bOk) { return Fail(Handle, Error); }
	if (Counted == 0) { return Fail(Handle, FString::Printf(TEXT("Selection '%s' holds no vertices"), *SelectionName)); }

	Stats.ValueSort([](const FBoneStat& A, const FBoneStat& B) { return A.Vertices > B.Vertices; });
	FString Lines;
	int32 Shown = 0;
	for (const TPair<int32, FBoneStat>& Pair : Stats)
	{
		if (Shown++ >= 20) { Lines += FString::Printf(TEXT("\n  (+%d more bones)"), Stats.Num() - 20); break; }
		const FString Name = BoneNames.IsValidIndex(Pair.Key) ? BoneNames[Pair.Key].ToString() : FString::Printf(TEXT("bone#%d"), Pair.Key);
		Lines += FString::Printf(TEXT("\n  %s: %d verts, avg %.3f, max %.3f"), *Name, Pair.Value.Vertices, Pair.Value.SumWeight / Pair.Value.Vertices, Pair.Value.MaxWeight);
	}
	return Ok(Handle, Mesh, FString::Printf(TEXT("%d vertices, %d influencing bone(s), weight sums %.3f..%.3f, max influences %d, %d vertices off-normalized (>1%% from 1.0):%s"),
		Counted, Stats.Num(), MinSum, MaxSum, MaxInfluences, OffNormalized, *Lines));
}

FModelingResult UModelingService::SetAssetMaterials(const FString& AssetPath, const FString& MaterialPaths, bool bSaveAsset)
{
	TArray<FString> Paths;
	MaterialPaths.ParseIntoArray(Paths, TEXT(","), true);
	TArray<UMaterialInterface*> Materials;
	for (FString& Path : Paths)
	{
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty()) { continue; }
		UMaterialInterface* Material = LoadAssetAs<UMaterialInterface>(Path);
		if (!Material) { return Fail(-1, FString::Printf(TEXT("Material not found: %s"), *Path)); }
		Materials.Add(Material);
	}
	if (Materials.Num() == 0) { return Fail(-1, TEXT("MaterialPaths must list at least one material asset path (comma-separated, in slot order)")); }

	UObject* Asset = UEditorAssetLibrary::DoesAssetExist(AssetPath) ? UEditorAssetLibrary::LoadAsset(AssetPath) : nullptr;
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		TArray<FStaticMaterial> Slots = StaticMesh->GetStaticMaterials();
		Slots.SetNum(FMath::Max(Slots.Num(), Materials.Num()));
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			Slots[i].MaterialInterface = Materials[i];
			if (Slots[i].MaterialSlotName.IsNone()) { Slots[i].MaterialSlotName = *FString::Printf(TEXT("Slot%d"), i); }
		}
		StaticMesh->SetStaticMaterials(Slots);
		StaticMesh->MarkPackageDirty();
		StaticMesh->PostEditChange();
	}
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
	{
		TArray<FSkeletalMaterial> Slots = SkeletalMesh->GetMaterials();
		Slots.SetNum(FMath::Max(Slots.Num(), Materials.Num()));
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			Slots[i].MaterialInterface = Materials[i];
			if (Slots[i].MaterialSlotName.IsNone()) { Slots[i].MaterialSlotName = *FString::Printf(TEXT("Slot%d"), i); }
		}
		SkeletalMesh->SetMaterials(Slots);
		SkeletalMesh->MarkPackageDirty();
		SkeletalMesh->PostEditChange();
	}
	else
	{
		return Fail(-1, FString::Printf(TEXT("%s is not a StaticMesh or SkeletalMesh asset"), *AssetPath));
	}
	SaveIf(bSaveAsset, AssetPath);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = AssetPath;
	R.Message = FString::Printf(TEXT("Assigned %d material slot(s) on %s"), Materials.Num(), *AssetPath);
	return R;
}

// =========================================================================
// UV control
// =========================================================================

namespace
{
	FGeometryScriptGroupLayer DefaultGroupLayer()
	{
		FGeometryScriptGroupLayer Layer;
		Layer.bDefaultLayer = true;
		return Layer;
	}

	/** Case-insensitive enum lookup so "Polygroups" finds PolyGroups. */
	template <typename TEnum>
	bool ParseEnumLoose(const FString& Name, TEnum& Out)
	{
		const UEnum* E = StaticEnum<TEnum>();
		const FString Wanted = Name.TrimStartAndEnd();
		for (int32 i = 0; i < E->NumEnums() - 1; ++i)
		{
			if (E->GetNameStringByIndex(i).Equals(Wanted, ESearchCase::IgnoreCase))
			{
				Out = static_cast<TEnum>(E->GetValueByIndex(i));
				return true;
			}
		}
		return false;
	}

	int32 NumUVLayers(UDynamicMesh* Mesh)
	{
		int32 Num = 0;
		Mesh->ProcessMesh([&Num](const FDynamicMesh3& M) { Num = M.HasAttributes() ? M.Attributes()->NumUVLayers() : 0; });
		return Num;
	}

	FGeometryScriptMeshSelection SelectionForMaterial(UDynamicMesh* Mesh, int32 MaterialID)
	{
		TArray<int32> TriangleIDs;
		Mesh->ProcessMesh([&](const FDynamicMesh3& M)
		{
			if (!M.HasAttributes() || !M.Attributes()->HasMaterialID()) { return; }
			const FDynamicMeshMaterialAttribute* Materials = M.Attributes()->GetMaterialID();
			for (const int32 Tid : M.TriangleIndicesItr())
			{
				if (Materials->GetValue(Tid) == MaterialID) { TriangleIDs.Add(Tid); }
			}
		});
		FGeometryScriptMeshSelection Selection;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(Mesh, TriangleIDs, EGeometryScriptMeshSelectionType::Triangles, Selection);
		return Selection;
	}
}

FModelingUVStats UModelingService::GetUVStats(int32 Handle, int32 UVLayer, int32 GridResolution)
{
	FModelingUVStats Stats;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { Stats.Message = NoMesh(Handle).Message; return Stats; }
	if (UVLayer < 0 || UVLayer >= NumUVLayers(Mesh)) { Stats.Message = FString::Printf(TEXT("UV layer %d does not exist (mesh has %d)"), UVLayer, NumUVLayers(Mesh)); return Stats; }

	FGeometryScriptMeshSelection All;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(Mesh, All, EGeometryScriptMeshSelectionType::Triangles);
	double MeshArea = 0.0, UVArea = 0.0;
	FBox MeshBounds(ForceInit);
	FBox2D UVBounds(ForceInit);
	bool bValid = false, bUnset = false;
	UGeometryScriptLibrary_MeshUVFunctions::GetMeshUVSizeInfo(Mesh, UVLayer, All, MeshArea, UVArea, MeshBounds, UVBounds, bValid, bUnset, true, nullptr);
	Stats.MeshArea = static_cast<float>(MeshArea);
	Stats.UVArea = static_cast<float>(UVArea);
	Stats.UVBoundsMin = UVBounds.Min;
	Stats.UVBoundsMax = UVBounds.Max;
	Stats.TexelsPerCmAt1K = MeshArea > 0.0 ? static_cast<float>(FMath::Sqrt(UVArea / MeshArea) * 1024.0) : 0.f;

	const int32 Res = FMath::Clamp(GridResolution, 16, 4096);
	TArray<uint8> Hits;
	Hits.SetNumZeroed(Res * Res);
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		const FDynamicMeshUVOverlay* UV = M.Attributes()->GetUVLayer(UVLayer);
		if (!UV) { return; }
		FMeshConnectedComponents Components(&M);
		Components.FindConnectedTriangles([UV](int32 T0, int32 T1) { return UV->AreTrianglesConnected(T0, T1); });
		Stats.NumIslands = Components.Num();
		for (const int32 Tid : M.TriangleIndicesItr())
		{
			if (!UV->IsSetTriangle(Tid)) { ++Stats.NumUnsetTriangles; continue; }
			FVector2f A, B, C;
			UV->GetTriElements(Tid, A, B, C);
			const FVector2f P0 = A * Res, P1 = B * Res, P2 = C * Res;
			const int32 X0 = FMath::Clamp(FMath::FloorToInt(FMath::Min3(P0.X, P1.X, P2.X)), 0, Res - 1), X1 = FMath::Clamp(FMath::CeilToInt(FMath::Max3(P0.X, P1.X, P2.X)), 0, Res - 1);
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt(FMath::Min3(P0.Y, P1.Y, P2.Y)), 0, Res - 1), Y1 = FMath::Clamp(FMath::CeilToInt(FMath::Max3(P0.Y, P1.Y, P2.Y)), 0, Res - 1);
			const float Area = (P1.X - P0.X) * (P2.Y - P0.Y) - (P2.X - P0.X) * (P1.Y - P0.Y);
			if (FMath::Abs(Area) < 1e-6f) { continue; }
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				for (int32 X = X0; X <= X1; ++X)
				{
					const FVector2f P(X + 0.5f, Y + 0.5f);
					const float W0 = ((P1.X - P.X) * (P2.Y - P.Y) - (P2.X - P.X) * (P1.Y - P.Y)) / Area;
					const float W1 = ((P2.X - P.X) * (P0.Y - P.Y) - (P0.X - P.X) * (P2.Y - P.Y)) / Area;
					const float W2 = 1.f - W0 - W1;
					if (W0 >= 0.f && W1 >= 0.f && W2 >= 0.f)
					{
						uint8& Cell = Hits[Y * Res + X];
						if (Cell < 255) { ++Cell; }
					}
				}
			}
		}
	});
	int32 Covered = 0, Overlapped = 0;
	for (const uint8 Cell : Hits)
	{
		if (Cell > 0) { ++Covered; }
		if (Cell > 1) { ++Overlapped; }
	}
	Stats.Coverage = static_cast<float>(Covered) / static_cast<float>(Res * Res);
	Stats.OverlapFraction = Covered > 0 ? static_cast<float>(Overlapped) / static_cast<float>(Covered) : 0.f;
	Stats.bSuccess = true;
	Stats.Message = FString::Printf(TEXT("%d island(s), %.0f%% of 0-1 covered, %.1f%% overlapping, %d unset triangle(s), %.2f texels/cm at 1K"),
		Stats.NumIslands, Stats.Coverage * 100.f, Stats.OverlapFraction * 100.f, Stats.NumUnsetTriangles, Stats.TexelsPerCmAt1K);
	return Stats;
}

// =========================================================================
// Image tools
// =========================================================================

namespace
{
	bool ParseCompression(const FString& Name, TextureCompressionSettings& Out)
	{
		const FString N = Name.TrimStartAndEnd();
		if (N.Equals(TEXT("Default"), ESearchCase::IgnoreCase)) { Out = TC_Default; return true; }
		if (N.Equals(TEXT("Normalmap"), ESearchCase::IgnoreCase)) { Out = TC_Normalmap; return true; }
		if (N.Equals(TEXT("Masks"), ESearchCase::IgnoreCase)) { Out = TC_Masks; return true; }
		if (N.Equals(TEXT("Grayscale"), ESearchCase::IgnoreCase)) { Out = TC_Grayscale; return true; }
		if (N.Equals(TEXT("HDR"), ESearchCase::IgnoreCase)) { Out = TC_HDR; return true; }
		return false;
	}

	/** Perlin fBm that wraps at the image borders (blend of the four torus-shifted lookups). */
	float TileableNoise(float U, float V, float Cells, const FVector2D& Offset)
	{
		const FVector2D P(U * Cells, V * Cells);
		const float N00 = FMath::PerlinNoise2D(P + Offset);
		const float N10 = FMath::PerlinNoise2D(P - FVector2D(Cells, 0) + Offset);
		const float N01 = FMath::PerlinNoise2D(P - FVector2D(0, Cells) + Offset);
		const float N11 = FMath::PerlinNoise2D(P - FVector2D(Cells, Cells) + Offset);
		return FMath::Lerp(FMath::Lerp(N00, N10, U), FMath::Lerp(N01, N11, U), V);
	}

	void FinishTextureEdit(UTexture2D* Texture, bool bSaveAsset)
	{
		Texture->UpdateResource();
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		Texture->BlockOnAnyAsyncBuild();
		SaveIf(bSaveAsset, Texture->GetPathName());
	}
}

FModelingResult UModelingService::CreateNoiseTexture(const FString& AssetPath, int32 Width, int32 Height, float Scale, int32 Octaves, float Persistence, int32 Seed, bool bSaveAsset)
{
	if (!AssetPath.StartsWith(TEXT("/"))) { return Fail(-1, FString::Printf(TEXT("AssetPath must be a content path like /Game/Textures/T_Name (got '%s')"), *AssetPath)); }
	const int32 W = FMath::Clamp(Width, 4, 8192), H = FMath::Clamp(Height, 4, 8192);
	const int32 NumOctaves = FMath::Clamp(Octaves, 1, 10);
	const float Cells = FMath::Max(1.f, Scale);
	FRandomStream Random(Seed);
	const FVector2D Offset(Random.FRandRange(0.f, 1000.f), Random.FRandRange(0.f, 1000.f));

	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(W * H * 4);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			float Value = 0.f, Amplitude = 1.f, Total = 0.f, Frequency = Cells;
			for (int32 O = 0; O < NumOctaves; ++O)
			{
				Value += Amplitude * TileableNoise(static_cast<float>(X) / W, static_cast<float>(Y) / H, Frequency, Offset * (O + 1));
				Total += Amplitude;
				Amplitude *= Persistence;
				Frequency *= 2.f;
			}
			const uint8 Gray = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((0.5f + 0.5f * Value / Total) * 255.f), 0, 255));
			uint8* P = &Pixels[(Y * W + X) * 4];
			P[0] = Gray; P[1] = Gray; P[2] = Gray; P[3] = 255;
		}
	}

	UTexture2D* Texture = UEditorAssetLibrary::DoesAssetExist(AssetPath) ? LoadAssetAs<UTexture2D>(AssetPath) : nullptr;
	if (!Texture)
	{
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath)) { return Fail(-1, FString::Printf(TEXT("%s exists and is not a Texture2D"), *AssetPath)); }
		UPackage* Package = CreatePackage(*AssetPath);
		Texture = NewObject<UTexture2D>(Package, *FPackageName::GetLongPackageAssetName(AssetPath), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Texture);
	}
	Texture->Source.Init(W, H, 1, 1, TSF_BGRA8, Pixels.GetData());
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_Grayscale;
	FinishTextureEdit(Texture, bSaveAsset);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = Texture->GetPathName();
	R.Message = FString::Printf(TEXT("Noise texture %dx%d (%d octaves, %.0f cells, seed %d) at %s"), W, H, NumOctaves, Cells, Seed, *R.AssetPath);
	return R;
}

FModelingResult UModelingService::DrawOnTexture(const FString& AssetPath, const FString& Shape, const TArray<FVector2D>& Points, FLinearColor Color, float ThicknessPx, float SpacingPx, bool bSaveAsset)
{
	UTexture2D* Texture = LoadAssetAs<UTexture2D>(AssetPath);
	if (!Texture) { return Fail(-1, FString::Printf(TEXT("Texture2D not found: %s"), *AssetPath)); }
	FTextureSource& Source = Texture->Source;
	if (!Source.IsValid()) { return Fail(-1, FString::Printf(TEXT("%s has no editable source data"), *AssetPath)); }
	const ETextureSourceFormat Format = Source.GetFormat();
	if (Format != TSF_BGRA8 && Format != TSF_G8)
	{
		return Fail(-1, FString::Printf(TEXT("%s is not an 8-bit texture (BGRA8 or G8); draw into an imported PNG or a create_noise_texture result"), *AssetPath));
	}
	const FString Kind = Shape.TrimStartAndEnd();
	const bool bLine = Kind.Equals(TEXT("Line"), ESearchCase::IgnoreCase), bDots = Kind.Equals(TEXT("Dots"), ESearchCase::IgnoreCase),
		bRect = Kind.Equals(TEXT("Rect"), ESearchCase::IgnoreCase), bFill = Kind.Equals(TEXT("Fill"), ESearchCase::IgnoreCase);
	if (!bLine && !bDots && !bRect && !bFill) { return Fail(-1, FString::Printf(TEXT("Unknown shape '%s' (use Line, Dots, Rect, Fill)"), *Shape)); }
	if ((bLine || bDots) && Points.Num() < 2) { return Fail(-1, TEXT("Line and Dots need at least 2 points")); }
	if (bRect && Points.Num() < 2) { return Fail(-1, TEXT("Rect needs 2 points (opposite corners)")); }

	const int32 W = static_cast<int32>(Source.GetSizeX()), H = static_cast<int32>(Source.GetSizeY());
	const FColor Pixel = Color.ToFColor(Texture->SRGB);
	const uint8 Gray = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(0.299f * Pixel.R + 0.587f * Pixel.G + 0.114f * Pixel.B), 0, 255));
	uint8* Data = Source.LockMip(0, 0, 0);
	if (!Data) { return Fail(-1, FString::Printf(TEXT("Could not lock %s for writing"), *AssetPath)); }
	int64 Painted = 0;
	auto Plot = [&](int32 X, int32 Y)
	{
		if (X < 0 || Y < 0 || X >= W || Y >= H) { return; }
		if (Format == TSF_BGRA8)
		{
			uint8* P = Data + (static_cast<int64>(Y) * W + X) * 4;
			P[0] = Pixel.B; P[1] = Pixel.G; P[2] = Pixel.R; P[3] = Pixel.A;
		}
		else
		{
			Data[static_cast<int64>(Y) * W + X] = Gray;
		}
		++Painted;
	};
	auto Disc = [&](const FVector2D& Center, float Radius)
	{
		const float R = FMath::Max(0.5f, Radius);
		for (int32 Y = FMath::FloorToInt(Center.Y - R); Y <= FMath::CeilToInt(Center.Y + R); ++Y)
		{
			for (int32 X = FMath::FloorToInt(Center.X - R); X <= FMath::CeilToInt(Center.X + R); ++X)
			{
				if (FVector2D::DistSquared(FVector2D(X + 0.5f, Y + 0.5f), Center) <= R * R) { Plot(X, Y); }
			}
		}
	};
	auto ToPixels = [&](const FVector2D& UV) { return FVector2D(UV.X * W, UV.Y * H); };

	if (bFill)
	{
		for (int32 Y = 0; Y < H; ++Y) { for (int32 X = 0; X < W; ++X) { Plot(X, Y); } }
	}
	else if (bRect)
	{
		const FVector2D A = ToPixels(Points[0]), B = ToPixels(Points[1]);
		for (int32 Y = FMath::FloorToInt(FMath::Min(A.Y, B.Y)); Y < FMath::CeilToInt(FMath::Max(A.Y, B.Y)); ++Y)
		{
			for (int32 X = FMath::FloorToInt(FMath::Min(A.X, B.X)); X < FMath::CeilToInt(FMath::Max(A.X, B.X)); ++X) { Plot(X, Y); }
		}
	}
	else
	{
		const float Radius = FMath::Max(0.5f, ThicknessPx * 0.5f);
		const float Step = bDots ? FMath::Max(1.f, SpacingPx) : 0.5f;
		float Carry = 0.f;   // distance already travelled since the last stamp, carried across segments
		bool bFirst = true;
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			const FVector2D A = ToPixels(Points[i]), B = ToPixels(Points[i + 1]);
			const float Length = FVector2D::Distance(A, B);
			if (Length < KINDA_SMALL_NUMBER) { continue; }
			const FVector2D Dir = (B - A) / Length;
			float T = bFirst ? 0.f : Step - Carry;
			bFirst = false;
			for (; T <= Length; T += Step) { Disc(A + Dir * T, Radius); }
			Carry = Length - (T - Step);
		}
		if (bLine) { Disc(ToPixels(Points.Last()), Radius); }
	}
	Source.UnlockMip(0, 0, 0);
	FinishTextureEdit(Texture, bSaveAsset);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = Texture->GetPathName();
	R.Message = FString::Printf(TEXT("Drew %s on %s (%lld pixel writes)"), *Kind, *AssetPath, Painted);
	return R;
}

FModelingResult UModelingService::AppendMeshAlongPolyline(int32 Handle, int32 OtherHandle, const TArray<FVector>& Points, float Spacing, FVector UpVector, float Scale)
{
	if (Points.Num() < 2) { return Fail(Handle, TEXT("Points needs at least 2 positions")); }
	if (Spacing <= 0.f) { return Fail(Handle, TEXT("Spacing must be positive")); }
	const FVector Up = UpVector.IsNearlyZero() ? FVector::UpVector : UpVector.GetSafeNormal();
	TArray<FTransform> Transforms;
	float Carry = 0.f;
	bool bFirst = true;
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const float Length = static_cast<float>(FVector::Distance(Points[i], Points[i + 1]));
		if (Length < KINDA_SMALL_NUMBER) { continue; }
		const FVector Dir = (Points[i + 1] - Points[i]) / Length;
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Dir, Up).ToQuat();
		float T = bFirst ? 0.f : Spacing - Carry;
		bFirst = false;
		for (; T <= Length + KINDA_SMALL_NUMBER; T += Spacing)
		{
			Transforms.Add(FTransform(Rotation, Points[i] + Dir * T, FVector(Scale)));
		}
		Carry = Length - (T - Spacing);
	}
	if (Transforms.Num() == 0) { return Fail(Handle, TEXT("Polyline is shorter than one Spacing")); }
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UDynamicMesh* Other = FindMesh(OtherHandle);
	if (!Other) { return NoMesh(OtherHandle); }
	if (Handle == OtherHandle) { return Fail(Handle, TEXT("A mesh cannot be stamped into itself - stamp a copy")); }
	UGeometryScriptDebug* Debug = NewDebug();
	for (const FTransform& T : Transforms)
	{
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, Other, T, true, FGeometryScriptAppendMeshOptions(), Debug);
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Stamped handle %d %d time(s) along a %d-point polyline every %.1f"), OtherHandle, Transforms.Num(), Points.Num(), Spacing));
}

FModelingResult UModelingService::CutGrooveAlongPolyline(int32 Handle, const TArray<FVector>& Points, float Width, float Depth, FVector UpVector)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (Points.Num() < 2) { return Fail(Handle, TEXT("Points needs at least 2 positions")); }
	if (Width <= 0.f || Depth <= 0.f) { return Fail(Handle, TEXT("Width and Depth must be positive")); }
	const FVector Up = UpVector.IsNearlyZero() ? FVector::UpVector : UpVector.GetSafeNormal();

	// One frame per point, X along the path (averaged at interior corners), Z along Up; the profile lives in the frame's YZ plane.
	TArray<FTransform> Frames;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		FVector Dir = FVector::ZeroVector;
		if (i > 0) { Dir += (Points[i] - Points[i - 1]).GetSafeNormal(); }
		if (i + 1 < Points.Num()) { Dir += (Points[i + 1] - Points[i]).GetSafeNormal(); }
		if (Dir.IsNearlyZero()) { continue; }
		Frames.Add(FTransform(FRotationMatrix::MakeFromXZ(Dir.GetSafeNormal(), Up).ToQuat(), Points[i]));
	}
	if (Frames.Num() < 2) { return Fail(Handle, TEXT("Polyline has fewer than 2 distinct points")); }
	const TArray<FVector2D> Profile = { FVector2D(-Width * 0.5, -Depth), FVector2D(Width * 0.5, -Depth), FVector2D(Width * 0.5, Depth), FVector2D(-Width * 0.5, Depth) };

	int32 ToolHandle = -1;
	UDynamicMesh* Tool = NewSessionMesh(ToolHandle);
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(Tool, PrimitiveOptions(0), FTransform::Identity, Profile, Frames, false, true, 1.f, 1.f, 0.f, 1.f, Debug);
	float Area = 0.f, Volume = 0.f;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(Tool, Area, Volume);
	if (Volume < 0.f) { UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Tool, Debug); }
	FGeometryScriptMeshBooleanOptions Options;
	Options.bFillHoles = true;
	Options.bSimplifyOutput = true;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(Mesh, FTransform::Identity, Tool, FTransform::Identity, EGeometryScriptBooleanOperation::Subtract, Options, Debug);
	ReleaseMesh(ToolHandle);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Cut a %.1f x %.1f groove along a %d-point polyline"), Width, Depth, Points.Num()));
}
