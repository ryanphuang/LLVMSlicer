add_library(LLVMWeiserSlicer SHARED
	Kleerer.cpp
	ModStats.cpp
	Slicing/FunctionStaticSlicer.cpp
	Slicing/PostDominanceFrontier.cpp
	Slicing/Prepare.cpp
	Slicing/StaticSlicer.cpp
	Callgraph/Callgraph.cpp
	Languages/LLVM.cpp
	Modifies/Modifies.cpp
	PointsTo/PointsTo.cpp
  Matcher/Scope.cpp
  #  Matcher/Matcher.cpp
)
