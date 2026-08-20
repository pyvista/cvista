"""
Utility module to make it easier to create new keys.
"""
from cvista.vtkCommonCore import vtkInformationDataObjectKey as DataaObjectKey
from cvista.vtkCommonCore import vtkInformationDoubleKey as DoubleKey
from cvista.vtkCommonCore import vtkInformationDoubleVectorKey as DoubleVectorKey
from cvista.vtkCommonCore import vtkInformationIdTypeKey as IdTypeKey
from cvista.vtkCommonCore import vtkInformationInformationKey as InformationKey
from cvista.vtkCommonCore import vtkInformationInformationVectorKey as InformationVectorKey
from cvista.vtkCommonCore import vtkInformationIntegerKey as IntegerKey
from cvista.vtkCommonCore import vtkInformationIntegerVectorKey as IntegerVectorKey
from cvista.vtkCommonCore import vtkInformationKeyVectorKey as KeyVectorKey
from cvista.vtkCommonCore import vtkInformationObjectBaseKey as ObjectBaseKey
from cvista.vtkCommonCore import vtkInformationObjectBaseVectorKey as ObjectBaseVectorKey
from cvista.vtkCommonCore import vtkInformationRequestKey as RequestKey
from cvista.vtkCommonCore import vtkInformationStringKey as StringKey
from cvista.vtkCommonCore import vtkInformationStringVectorKey as StringVectorKey
from cvista.vtkCommonCore import vtkInformationUnsignedLongKey as UnsignedLongKey
from cvista.vtkCommonCore import vtkInformationVariantKey as VariantKey
from cvista.vtkCommonCore import vtkInformationVariantVectorKey as VariantVectorKey
from cvista.vtkCommonExecutionModel import vtkInformationExecutivePortKey as ExecutivePortKey
from cvista.vtkCommonExecutionModel import vtkInformationExecutivePortVectorKey as ExecutivePortVectorKey

# cvista trims these two key types from the build (cvista-config/_nowrap_classes.cmake,
# and DataObjectMetaDataKey is also in _nocompile_classes.cmake), so they are absent
# from the wrapped vtkCommonExecutionModel. Importing them unconditionally made this
# whole module fail to import -- taking MakeKey and every other key type down with it.
# Bind them to None when absent so the module stays usable in a trimmed build.
try:
    from cvista.vtkCommonExecutionModel import vtkInformationDataObjectMetaDataKey as DataObjectMetaDataKey
except ImportError:  # pragma: no cover - depends on build configuration
    DataObjectMetaDataKey = None
try:
    from cvista.vtkCommonExecutionModel import vtkInformationIntegerRequestKey as IntegerRequestKey
except ImportError:  # pragma: no cover - depends on build configuration
    IntegerRequestKey = None

def MakeKey(key_type, name, location, *args):
    """Given a key type, make a new key of given name
    and location."""
    return key_type.MakeKey(name, location, *args)
