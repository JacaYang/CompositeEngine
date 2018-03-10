#include "AssetExporter.h"

#include "AssetSerializer.h"
#include "OutputFileStream.h"


namespace CE
{
	bool AssetExporter::ExportSkeletonMeshesAnimationsTexture(
		const char* fileName,
		const Skeleton& skeleton,
		const Meshes& meshes,
		const Animations& animations,
		const Texture& texture)
	{
		OutputFileStream stream(fileName);

		if (!stream.IsValid())
		{
			return false;
		}

		AssetSerializer serializer(stream);
		serializer.WriteHeader();
		serializer.WriteSkeleton(skeleton);
		serializer.WriteMeshes(meshes);
		serializer.WriteAnimations(animations);
		serializer.WriteTexture(texture);

		return stream.IsValid();
	}
}
