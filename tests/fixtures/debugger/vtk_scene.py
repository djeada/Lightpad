"""Real VTK pipeline and zero-copy NumPy data for debugger inspection."""

from vtkmodules.util.numpy_support import vtk_to_numpy
from vtkmodules.vtkCommonCore import vtkFloatArray
from vtkmodules.vtkFiltersSources import vtkSphereSource
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkPolyDataMapper,
    vtkRenderer,
    vtkRenderWindow,
)
import vtkmodules.vtkRenderingOpenGL2  # Register the OpenGL rendering backend.

sphere = vtkSphereSource()
sphere.SetThetaResolution(16)
sphere.SetPhiResolution(16)
sphere.Update()
mesh = sphere.GetOutput()
vectors = vtkFloatArray()
vectors.SetName("velocity")
vectors.SetNumberOfComponents(3)
vectors.SetNumberOfTuples(mesh.GetNumberOfPoints())
for index in range(mesh.GetNumberOfPoints()):
    vectors.SetTuple3(index, index + 0.25, index + 0.5, index + 0.75)
mesh.GetPointData().SetVectors(vectors)
coordinates = vtk_to_numpy(mesh.GetPoints().GetData())
velocity = vtk_to_numpy(vectors)
point_rows = [mesh.GetPoint(index) for index in range(8)]
mapper = vtkPolyDataMapper()
mapper.SetInputData(mesh)
actor = vtkActor()
actor.SetMapper(mapper)
renderer = vtkRenderer()
renderer.AddActor(actor)
window = vtkRenderWindow()
window.SetOffScreenRendering(True)
window.SetSize(64, 64)
window.AddRenderer(renderer)
window.Render()
coordinates[0, 0] = 99.0  # INSPECT: pause after rendering, before mutation.
mesh.Modified()
print("vtk-debug-ok", mesh.GetNumberOfPoints(), mesh.GetPoint(0)[0])
window.Finalize()
