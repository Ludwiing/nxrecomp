// Generado por tools/gen_maxwell.py a partir del traductor de shaders de Ryujinx (MIT,
// third_party/LICENSE.ryujinx). No editar a mano.
#pragma once
#include <stdint.h>

enum { MX_ASelect4_0000 = 0, MX_ASelect4_1111 = 1, MX_ASelect4_2222 = 2, MX_ASelect4_3333 = 3, MX_ASelect4_3210 = 4, MX_ASelect4_5432 = 6, MX_ASelect4_6543 = 7, MX_ASelect4_3201 = 8, MX_ASelect4_3012 = 9, MX_ASelect4_0213 = 10, MX_ASelect4_3120 = 11, MX_ASelect4_1230 = 12, MX_ASelect4_2310 = 13 };
enum { MX_AddressMode_Il = 1, MX_AddressMode_Is = 2, MX_AddressMode_Isl = 3 };
enum { MX_AlSize_32 = 0, MX_AlSize_64 = 1, MX_AlSize_96 = 2, MX_AlSize_128 = 3 };
enum { MX_AtomOp_Add = 0, MX_AtomOp_Min = 1, MX_AtomOp_Max = 2, MX_AtomOp_Inc = 3, MX_AtomOp_Dec = 4, MX_AtomOp_And = 5, MX_AtomOp_Or = 6, MX_AtomOp_Xor = 7, MX_AtomOp_Exch = 8, MX_AtomOp_Safeadd = 10 };
enum { MX_AtomSize_U32 = 0, MX_AtomSize_S32 = 1, MX_AtomSize_U64 = 2, MX_AtomSize_F32FtzRn = 3, MX_AtomSize_F16x2FtzRn = 4, MX_AtomSize_S64 = 5 };
enum { MX_AtomsSize_U32 = 0, MX_AtomsSize_S32 = 1, MX_AtomsSize_U64 = 2, MX_AtomsSize_S64 = 3 };
enum { MX_AvgMode_NoNeg = 0, MX_AvgMode_NegB = 1, MX_AvgMode_NegA = 2, MX_AvgMode_PlusOne = 3 };
enum { MX_BSelect4_4444 = 0, MX_BSelect4_5555 = 1, MX_BSelect4_6666 = 2, MX_BSelect4_7777 = 3, MX_BSelect4_7654 = 4, MX_BSelect4_5432 = 6, MX_BSelect4_4321 = 7, MX_BSelect4_4567 = 8, MX_BSelect4_6745 = 9, MX_BSelect4_5476 = 10 };
enum { MX_BarMode_Bar = 0, MX_BarMode_Result = 1, MX_BarMode_Warp = 2 };
enum { MX_BarOp_Sync = 0, MX_BarOp_Arv = 1, MX_BarOp_Red = 2, MX_BarOp_Scan = 3, MX_BarOp_SyncAll = 4 };
enum { MX_BarRedOp_Popc = 0, MX_BarRedOp_And = 1, MX_BarRedOp_Or = 2 };
enum { MX_BoolOp_And = 0, MX_BoolOp_Or = 1, MX_BoolOp_Xor = 2 };
enum { MX_Bpt_DrainIllegal = 0, MX_Bpt_Cal = 1, MX_Bpt_Pause = 2, MX_Bpt_Trap = 3, MX_Bpt_Int = 4, MX_Bpt_Drain = 5 };
enum { MX_ByteSel_B0 = 0, MX_ByteSel_B1 = 1, MX_ByteSel_B2 = 2, MX_ByteSel_B3 = 3 };
enum { MX_CacheOp2_Lu = 1, MX_CacheOp2_Ci = 2, MX_CacheOp2_Cv = 3 };
enum { MX_CacheOpLd_Ca = 0, MX_CacheOpLd_Cg = 1, MX_CacheOpLd_Ci = 2, MX_CacheOpLd_Cv = 3 };
enum { MX_CacheOpSt_Wb = 0, MX_CacheOpSt_Cg = 1, MX_CacheOpSt_Ci = 2, MX_CacheOpSt_Wt = 3 };
enum { MX_CacheType_U = 1, MX_CacheType_C = 2, MX_CacheType_I = 3, MX_CacheType_Crs = 4 };
enum { MX_Ccc_F = 0, MX_Ccc_Lt = 1, MX_Ccc_Eq = 2, MX_Ccc_Le = 3, MX_Ccc_Gt = 4, MX_Ccc_Ne = 5, MX_Ccc_Ge = 6, MX_Ccc_Num = 7, MX_Ccc_Nan = 8, MX_Ccc_Ltu = 9, MX_Ccc_Equ = 10, MX_Ccc_Leu = 11, MX_Ccc_Gtu = 12, MX_Ccc_Neu = 13, MX_Ccc_Geu = 14, MX_Ccc_T = 15, MX_Ccc_Off = 16, MX_Ccc_Lo = 17, MX_Ccc_Sff = 18, MX_Ccc_Ls = 19, MX_Ccc_Hi = 20, MX_Ccc_Sft = 21, MX_Ccc_Hs = 22, MX_Ccc_Oft = 23, MX_Ccc_CsmTa = 24, MX_Ccc_CsmTr = 25, MX_Ccc_CsmMx = 26, MX_Ccc_FcsmTa = 27, MX_Ccc_FcsmTr = 28, MX_Ccc_FcsmMx = 29, MX_Ccc_Rle = 30, MX_Ccc_Rgt = 31 };
enum { MX_CctlOp_Pf1 = 1, MX_CctlOp_Pf1_5 = 2, MX_CctlOp_Pf2 = 3, MX_CctlOp_Wb = 4, MX_CctlOp_Iv = 5, MX_CctlOp_Ivall = 6, MX_CctlOp_Rs = 7, MX_CctlOp_Rslb = 9 };
enum { MX_CctltOp_Ivth = 1 };
enum { MX_ChkModeF_Divide = 0 };
enum { MX_Clamp_Ign = 0, MX_Clamp_Trap = 2 };
enum { MX_DstFmt_F16 = 1, MX_DstFmt_F32 = 2, MX_DstFmt_F64 = 3 };
enum { MX_FComp_F = 0, MX_FComp_Lt = 1, MX_FComp_Eq = 2, MX_FComp_Le = 3, MX_FComp_Gt = 4, MX_FComp_Ne = 5, MX_FComp_Ge = 6, MX_FComp_Num = 7, MX_FComp_Nan = 8, MX_FComp_Ltu = 9, MX_FComp_Equ = 10, MX_FComp_Leu = 11, MX_FComp_Gtu = 12, MX_FComp_Neu = 13, MX_FComp_Geu = 14, MX_FComp_T = 15 };
enum { MX_Fmz_Ftz = 1, MX_Fmz_Fmz = 2 };
enum { MX_HalfSelect_B32 = 0, MX_HalfSelect_H0 = 1, MX_HalfSelect_H1 = 2 };
enum { MX_HalfSwizzle_F16 = 0, MX_HalfSwizzle_F32 = 1, MX_HalfSwizzle_H0H0 = 2, MX_HalfSwizzle_H1H1 = 3 };
enum { MX_IBase_Patch = 1, MX_IBase_Prim = 2, MX_IBase_Attr = 3 };
enum { MX_IComp_F = 0, MX_IComp_Lt = 1, MX_IComp_Eq = 2, MX_IComp_Le = 3, MX_IComp_Gt = 4, MX_IComp_Ne = 5, MX_IComp_Ge = 6, MX_IComp_T = 7 };
enum { MX_IDstFmt_U16 = 1, MX_IDstFmt_U32 = 2, MX_IDstFmt_U64 = 3, MX_IDstFmt_S16 = 5, MX_IDstFmt_S32 = 6, MX_IDstFmt_S64 = 7 };
enum { MX_ISrcDstFmt_U8 = 0, MX_ISrcDstFmt_U16 = 1, MX_ISrcDstFmt_U32 = 2, MX_ISrcDstFmt_S8 = 4, MX_ISrcDstFmt_S16 = 5, MX_ISrcDstFmt_S32 = 6 };
enum { MX_ISrcFmt_U8 = 0, MX_ISrcFmt_U16 = 1, MX_ISrcFmt_U32 = 2, MX_ISrcFmt_U64 = 3, MX_ISrcFmt_S8 = 4, MX_ISrcFmt_S16 = 5, MX_ISrcFmt_S32 = 6, MX_ISrcFmt_S64 = 7 };
enum { MX_ImadspASelect_U32 = 0, MX_ImadspASelect_S32 = 1, MX_ImadspASelect_U24 = 2, MX_ImadspASelect_S24 = 3, MX_ImadspASelect_U16h0 = 4, MX_ImadspASelect_S16h0 = 5, MX_ImadspASelect_U16h1 = 6, MX_ImadspASelect_S16h1 = 7 };
enum { MX_ImadspBSelect_U24 = 0, MX_ImadspBSelect_S24 = 1, MX_ImadspBSelect_U16h0 = 2, MX_ImadspBSelect_S16h0 = 3 };
enum { MX_IntegerRound_Pass = 1, MX_IntegerRound_Round = 4, MX_IntegerRound_Floor = 5, MX_IntegerRound_Ceil = 6, MX_IntegerRound_Trunc = 7 };
enum { MX_IpaOp_Pass = 0, MX_IpaOp_Multiply = 1, MX_IpaOp_Constant = 2, MX_IpaOp_Sc = 3 };
enum { MX_Ivall_Ivalld = 1, MX_Ivall_Ivallt = 2, MX_Ivall_Ivalltd = 3 };
enum { MX_LaneMask4_Z = 1, MX_LaneMask4_W = 2, MX_LaneMask4_Zw = 3, MX_LaneMask4_X = 4, MX_LaneMask4_Xz = 5, MX_LaneMask4_Xw = 6, MX_LaneMask4_Xzw = 7, MX_LaneMask4_Y = 8, MX_LaneMask4_Yz = 9, MX_LaneMask4_Yw = 10, MX_LaneMask4_Yzw = 11, MX_LaneMask4_Xy = 12, MX_LaneMask4_Xyz = 13, MX_LaneMask4_Xyw = 14, MX_LaneMask4_Xyzw = 15 };
enum { MX_Lod_Lz = 1, MX_Lod_Lb = 2, MX_Lod_Ll = 3, MX_Lod_Lba = 6, MX_Lod_Lla = 7 };
enum { MX_LogicOp_And = 0, MX_LogicOp_Or = 1, MX_LogicOp_Xor = 2, MX_LogicOp_PassB = 3 };
enum { MX_Lrs_None = 0, MX_Lrs_RightShift = 1, MX_Lrs_LeftShift = 2 };
enum { MX_LsSize_U8 = 0, MX_LsSize_S8 = 1, MX_LsSize_U16 = 2, MX_LsSize_S16 = 3, MX_LsSize_B32 = 4, MX_LsSize_B64 = 5, MX_LsSize_B128 = 6, MX_LsSize_UB128 = 7 };
enum { MX_LsSize2_U8 = 0, MX_LsSize2_S8 = 1, MX_LsSize2_U16 = 2, MX_LsSize2_S16 = 3, MX_LsSize2_B32 = 4, MX_LsSize2_B64 = 5, MX_LsSize2_B128 = 6 };
enum { MX_MaxShift_U64 = 2, MX_MaxShift_S64 = 3 };
enum { MX_Membar_Cta = 0, MX_Membar_Gl = 1, MX_Membar_Sys = 2, MX_Membar_Vc = 3 };
enum { MX_MufuOp_Cos = 0, MX_MufuOp_Sin = 1, MX_MufuOp_Ex2 = 2, MX_MufuOp_Lg2 = 3, MX_MufuOp_Rcp = 4, MX_MufuOp_Rsq = 5, MX_MufuOp_Rcp64h = 6, MX_MufuOp_Rsq64h = 7, MX_MufuOp_Sqrt = 8 };
enum { MX_MultiplyScale_NoScale = 0, MX_MultiplyScale_D2 = 1, MX_MultiplyScale_D4 = 2, MX_MultiplyScale_D8 = 3, MX_MultiplyScale_M8 = 4, MX_MultiplyScale_M4 = 5, MX_MultiplyScale_M2 = 6 };
enum { MX_OFmt_F16 = 0, MX_OFmt_F32 = 1, MX_OFmt_MrgH0 = 2, MX_OFmt_MrgH1 = 3 };
enum { MX_OutType_Emit = 1, MX_OutType_Cut = 2, MX_OutType_EmitThenCut = 3 };
enum { MX_PMode_F4e = 1, MX_PMode_B4e = 2, MX_PMode_Rc8 = 3, MX_PMode_Ecl = 4, MX_PMode_Ecr = 5, MX_PMode_Rc16 = 6 };
enum { MX_PixMode_Covmask = 1, MX_PixMode_Covered = 2, MX_PixMode_Offset = 3, MX_PixMode_CentroidOffset = 4, MX_PixMode_MyIndex = 5 };
enum { MX_PredicateOp_F = 0, MX_PredicateOp_T = 1, MX_PredicateOp_Z = 2, MX_PredicateOp_Nz = 3 };
enum { MX_RedOp_Add = 0, MX_RedOp_Min = 1, MX_RedOp_Max = 2, MX_RedOp_Inc = 3, MX_RedOp_Dec = 4, MX_RedOp_And = 5, MX_RedOp_Or = 6, MX_RedOp_Xor = 7 };
enum { MX_RoundMode_Rn = 0, MX_RoundMode_Rm = 1, MX_RoundMode_Rp = 2, MX_RoundMode_Rz = 3 };
enum { MX_RoundMode2_Round = 0, MX_RoundMode2_Floor = 1, MX_RoundMode2_Ceil = 2, MX_RoundMode2_Trunc = 3 };
enum { MX_SReg_LaneId = 0, MX_SReg_Clock = 1, MX_SReg_VirtCfg = 2, MX_SReg_VirtId = 3, MX_SReg_Pm0 = 4, MX_SReg_Pm1 = 5, MX_SReg_Pm2 = 6, MX_SReg_Pm3 = 7, MX_SReg_Pm4 = 8, MX_SReg_Pm5 = 9, MX_SReg_Pm6 = 10, MX_SReg_Pm7 = 11, MX_SReg_OrderingTicket = 15, MX_SReg_PrimType = 16, MX_SReg_InvocationId = 17, MX_SReg_YDirection = 18, MX_SReg_ThreadKill = 19, MX_SReg_ShaderType = 20, MX_SReg_DirectCbeWriteAddressLow = 21, MX_SReg_DirectCbeWriteAddressHigh = 22, MX_SReg_DirectCbeWriteEnabled = 23, MX_SReg_MachineId0 = 24, MX_SReg_MachineId1 = 25, MX_SReg_MachineId2 = 26, MX_SReg_MachineId3 = 27, MX_SReg_Affinity = 28, MX_SReg_InvocationInfo = 29, MX_SReg_WScaleFactorXY = 30, MX_SReg_WScaleFactorZ = 31, MX_SReg_TId = 32, MX_SReg_TIdX = 33, MX_SReg_TIdY = 34, MX_SReg_TIdZ = 35, MX_SReg_CtaParam = 36, MX_SReg_CtaIdX = 37, MX_SReg_CtaIdY = 38, MX_SReg_CtaIdZ = 39, MX_SReg_Ntid = 40, MX_SReg_CirQueueIncrMinusOne = 41, MX_SReg_Nlatc = 42, MX_SReg_Swinlo = 48, MX_SReg_Swinsz = 49, MX_SReg_Smemsz = 50, MX_SReg_Smembanks = 51, MX_SReg_LWinLo = 52, MX_SReg_LWinSz = 53, MX_SReg_LMemLoSz = 54, MX_SReg_LMemHiOff = 55, MX_SReg_EqMask = 56, MX_SReg_LtMask = 57, MX_SReg_LeMask = 58, MX_SReg_GtMask = 59, MX_SReg_GeMask = 60, MX_SReg_RegAlloc = 61, MX_SReg_CtxAddr = 62, MX_SReg_GlobalErrorStatus = 64, MX_SReg_WarpErrorStatus = 66, MX_SReg_WarpErrorStatusClear = 67, MX_SReg_PmHi0 = 72, MX_SReg_PmHi1 = 73, MX_SReg_PmHi2 = 74, MX_SReg_PmHi3 = 75, MX_SReg_PmHi4 = 76, MX_SReg_PmHi5 = 77, MX_SReg_PmHi6 = 78, MX_SReg_PmHi7 = 79, MX_SReg_ClockLo = 80, MX_SReg_ClockHi = 81, MX_SReg_GlobalTimerLo = 82, MX_SReg_GlobalTimerHi = 83, MX_SReg_HwTaskId = 96, MX_SReg_CircularQueueEntryIndex = 97, MX_SReg_CircularQueueEntryAddressLow = 98, MX_SReg_CircularQueueEntryAddressHigh = 99 };
enum { MX_ShflMode_Idx = 0, MX_ShflMode_Up = 1, MX_ShflMode_Down = 2, MX_ShflMode_Bfly = 3 };
enum { MX_SuDim_1d = 0, MX_SuDim_1dBuffer = 1, MX_SuDim_1dArray = 2, MX_SuDim_2d = 3, MX_SuDim_2dArray = 4, MX_SuDim_3d = 5 };
enum { MX_SuRgba_R = 1, MX_SuRgba_G = 2, MX_SuRgba_Rg = 3, MX_SuRgba_B = 4, MX_SuRgba_Rb = 5, MX_SuRgba_Gb = 6, MX_SuRgba_Rgb = 7, MX_SuRgba_A = 8, MX_SuRgba_Ra = 9, MX_SuRgba_Ga = 10, MX_SuRgba_Rga = 11, MX_SuRgba_Ba = 12, MX_SuRgba_Rba = 13, MX_SuRgba_Gba = 14, MX_SuRgba_Rgba = 15 };
enum { MX_SuSize_U8 = 0, MX_SuSize_S8 = 1, MX_SuSize_U16 = 2, MX_SuSize_S16 = 3, MX_SuSize_B32 = 4, MX_SuSize_B64 = 5, MX_SuSize_B128 = 6, MX_SuSize_UB128 = 7 };
enum { MX_SuatomOp_Add = 0, MX_SuatomOp_Min = 1, MX_SuatomOp_Max = 2, MX_SuatomOp_Inc = 3, MX_SuatomOp_Dec = 4, MX_SuatomOp_And = 5, MX_SuatomOp_Or = 6, MX_SuatomOp_Xor = 7, MX_SuatomOp_Exch = 8 };
enum { MX_SuatomSize_U32 = 0, MX_SuatomSize_S32 = 1, MX_SuatomSize_U64 = 2, MX_SuatomSize_F32FtzRn = 3, MX_SuatomSize_F16x2FtzRn = 4, MX_SuatomSize_S64 = 5, MX_SuatomSize_Sd32 = 6, MX_SuatomSize_Sd64 = 7 };
enum { MX_TexComp_R = 0, MX_TexComp_G = 1, MX_TexComp_B = 2, MX_TexComp_A = 3 };
enum { MX_TexDim_1d = 0, MX_TexDim_Array1d = 1, MX_TexDim_2d = 2, MX_TexDim_Array2d = 3, MX_TexDim_3d = 4, MX_TexDim_Array3d = 5, MX_TexDim_Cube = 6, MX_TexDim_ArrayCube = 7 };
enum { MX_TexOffset_None = 0, MX_TexOffset_Aoffi = 1, MX_TexOffset_Ptp = 2 };
enum { MX_TexQuery_TexHeaderDimension = 1, MX_TexQuery_TexHeaderTextureType = 2, MX_TexQuery_TexHeaderSamplerPos = 5, MX_TexQuery_TexSamplerFilter = 16, MX_TexQuery_TexSamplerLod = 18, MX_TexQuery_TexSamplerWrap = 20, MX_TexQuery_TexSamplerBorderColor = 22 };
enum { MX_TexsTarget_Texture1DLodZero = 0, MX_TexsTarget_Texture2D = 1, MX_TexsTarget_Texture2DLodZero = 2, MX_TexsTarget_Texture2DLodLevel = 3, MX_TexsTarget_Texture2DDepthCompare = 4, MX_TexsTarget_Texture2DLodLevelDepthCompare = 5, MX_TexsTarget_Texture2DLodZeroDepthCompare = 6, MX_TexsTarget_Texture2DArray = 7, MX_TexsTarget_Texture2DArrayLodZero = 8, MX_TexsTarget_Texture2DArrayLodZeroDepthCompare = 9, MX_TexsTarget_Texture3D = 10, MX_TexsTarget_Texture3DLodZero = 11, MX_TexsTarget_TextureCube = 12, MX_TexsTarget_TextureCubeLodLevel = 13 };
enum { MX_TldsTarget_Texture1DLodZero = 0, MX_TldsTarget_Texture1DLodLevel = 1, MX_TldsTarget_Texture2DLodZero = 2, MX_TldsTarget_Texture2DLodZeroOffset = 4, MX_TldsTarget_Texture2DLodLevel = 5, MX_TldsTarget_Texture2DLodZeroMultisample = 6, MX_TldsTarget_Texture3DLodZero = 7, MX_TldsTarget_Texture2DArrayLodZero = 8, MX_TldsTarget_Texture2DLodLevelOffset = 12 };
enum { MX_VectorSelect_U8B0 = 0, MX_VectorSelect_U8B1 = 1, MX_VectorSelect_U8B2 = 2, MX_VectorSelect_U8B3 = 3, MX_VectorSelect_U16H0 = 4, MX_VectorSelect_U16H1 = 5, MX_VectorSelect_U32 = 6, MX_VectorSelect_S8B0 = 8, MX_VectorSelect_S8B1 = 9, MX_VectorSelect_S8B2 = 10, MX_VectorSelect_S8B3 = 11, MX_VectorSelect_S16H0 = 12, MX_VectorSelect_S16H1 = 13, MX_VectorSelect_S32 = 14 };
enum { MX_VideoOp_Mrg16h = 0, MX_VideoOp_Mrg16l = 1, MX_VideoOp_Mrg8b0 = 2, MX_VideoOp_Mrg8b2 = 3, MX_VideoOp_Acc = 4, MX_VideoOp_Min = 5, MX_VideoOp_Max = 6 };
enum { MX_VideoRed_Acc = 1 };
enum { MX_VideoScale_Shr7 = 1, MX_VideoScale_Shr15 = 2 };
enum { MX_VoteMode_All = 0, MX_VoteMode_Any = 1, MX_VoteMode_Eq = 2 };
enum { MX_XMode_Xlo = 1, MX_XMode_Xmed = 2, MX_XMode_Xhi = 3 };
enum { MX_XModeShf_Hi = 1, MX_XModeShf_X = 2, MX_XModeShf_Xhi = 3 };
enum { MX_XmadCop_Cfull = 0, MX_XmadCop_Clo = 1, MX_XmadCop_Chi = 2, MX_XmadCop_Csfu = 3, MX_XmadCop_Cbcc = 4 };
enum { MX_XmadCop2_Cfull = 0, MX_XmadCop2_Clo = 1, MX_XmadCop2_Chi = 2, MX_XmadCop2_Csfu = 3 };

enum MxOp {
    MXOP_INVALID,
    MXOP_Al2p,
    MXOP_Ald,
    MXOP_Ast,
    MXOP_Atom,
    MXOP_AtomCas,
    MXOP_Atoms,
    MXOP_AtomsCas,
    MXOP_B2r,
    MXOP_Bar,
    MXOP_BfeC,
    MXOP_BfeI,
    MXOP_BfeR,
    MXOP_BfiC,
    MXOP_BfiI,
    MXOP_BfiR,
    MXOP_BfiRc,
    MXOP_Bpt,
    MXOP_Bra,
    MXOP_Brk,
    MXOP_Brx,
    MXOP_Cal,
    MXOP_Cctl,
    MXOP_Cctll,
    MXOP_Cctlt,
    MXOP_Cont,
    MXOP_Cs2r,
    MXOP_Cset,
    MXOP_Csetp,
    MXOP_DaddC,
    MXOP_DaddI,
    MXOP_DaddR,
    MXOP_Depbar,
    MXOP_DfmaC,
    MXOP_DfmaI,
    MXOP_DfmaR,
    MXOP_DfmaRc,
    MXOP_DmnmxC,
    MXOP_DmnmxI,
    MXOP_DmnmxR,
    MXOP_DmulC,
    MXOP_DmulI,
    MXOP_DmulR,
    MXOP_DsetC,
    MXOP_DsetI,
    MXOP_DsetR,
    MXOP_DsetpC,
    MXOP_DsetpI,
    MXOP_DsetpR,
    MXOP_Exit,
    MXOP_F2fC,
    MXOP_F2fI,
    MXOP_F2fR,
    MXOP_F2iC,
    MXOP_F2iI,
    MXOP_F2iR,
    MXOP_Fadd32i,
    MXOP_FaddC,
    MXOP_FaddI,
    MXOP_FaddR,
    MXOP_FchkC,
    MXOP_FchkI,
    MXOP_FchkR,
    MXOP_FcmpC,
    MXOP_FcmpI,
    MXOP_FcmpR,
    MXOP_FcmpRc,
    MXOP_Ffma32i,
    MXOP_FfmaC,
    MXOP_FfmaI,
    MXOP_FfmaR,
    MXOP_FfmaRc,
    MXOP_FloC,
    MXOP_FloI,
    MXOP_FloR,
    MXOP_FmnmxC,
    MXOP_FmnmxI,
    MXOP_FmnmxR,
    MXOP_Fmul32i,
    MXOP_FmulC,
    MXOP_FmulI,
    MXOP_FmulR,
    MXOP_FsetC,
    MXOP_FsetI,
    MXOP_FsetR,
    MXOP_FsetpC,
    MXOP_FsetpI,
    MXOP_FsetpR,
    MXOP_Fswzadd,
    MXOP_Getcrsptr,
    MXOP_Getlmembase,
    MXOP_Hadd232i,
    MXOP_Hadd2C,
    MXOP_Hadd2I,
    MXOP_Hadd2R,
    MXOP_Hfma232i,
    MXOP_Hfma2C,
    MXOP_Hfma2I,
    MXOP_Hfma2R,
    MXOP_Hfma2Rc,
    MXOP_Hmul232i,
    MXOP_Hmul2C,
    MXOP_Hmul2I,
    MXOP_Hmul2R,
    MXOP_Hset2C,
    MXOP_Hset2I,
    MXOP_Hset2R,
    MXOP_Hsetp2C,
    MXOP_Hsetp2I,
    MXOP_Hsetp2R,
    MXOP_I2fC,
    MXOP_I2fI,
    MXOP_I2fR,
    MXOP_I2iC,
    MXOP_I2iI,
    MXOP_I2iR,
    MXOP_Iadd32i,
    MXOP_Iadd3C,
    MXOP_Iadd3I,
    MXOP_Iadd3R,
    MXOP_IaddC,
    MXOP_IaddI,
    MXOP_IaddR,
    MXOP_IcmpC,
    MXOP_IcmpI,
    MXOP_IcmpR,
    MXOP_IcmpRc,
    MXOP_Ide,
    MXOP_IdpC,
    MXOP_IdpR,
    MXOP_Imad32i,
    MXOP_ImadC,
    MXOP_ImadI,
    MXOP_ImadR,
    MXOP_ImadRc,
    MXOP_ImadspC,
    MXOP_ImadspI,
    MXOP_ImadspR,
    MXOP_ImadspRc,
    MXOP_ImnmxC,
    MXOP_ImnmxI,
    MXOP_ImnmxR,
    MXOP_Imul32i,
    MXOP_ImulC,
    MXOP_ImulI,
    MXOP_ImulR,
    MXOP_Ipa,
    MXOP_Isberd,
    MXOP_Iscadd32i,
    MXOP_IscaddC,
    MXOP_IscaddI,
    MXOP_IscaddR,
    MXOP_IsetC,
    MXOP_IsetI,
    MXOP_IsetR,
    MXOP_IsetpC,
    MXOP_IsetpI,
    MXOP_IsetpR,
    MXOP_Jcal,
    MXOP_Jmp,
    MXOP_Jmx,
    MXOP_Kil,
    MXOP_Ld,
    MXOP_Ldc,
    MXOP_Ldg,
    MXOP_Ldl,
    MXOP_Lds,
    MXOP_LeaC,
    MXOP_LeaHiC,
    MXOP_LeaHiR,
    MXOP_LeaI,
    MXOP_LeaR,
    MXOP_Lepc,
    MXOP_Longjmp,
    MXOP_Lop32i,
    MXOP_Lop3C,
    MXOP_Lop3I,
    MXOP_Lop3R,
    MXOP_LopC,
    MXOP_LopI,
    MXOP_LopR,
    MXOP_Membar,
    MXOP_Mov32i,
    MXOP_MovC,
    MXOP_MovI,
    MXOP_MovR,
    MXOP_Mufu,
    MXOP_Nop,
    MXOP_OutC,
    MXOP_OutI,
    MXOP_OutR,
    MXOP_P2rC,
    MXOP_P2rI,
    MXOP_P2rR,
    MXOP_Pbk,
    MXOP_Pcnt,
    MXOP_Pexit,
    MXOP_Pixld,
    MXOP_Plongjmp,
    MXOP_PopcC,
    MXOP_PopcI,
    MXOP_PopcR,
    MXOP_Pret,
    MXOP_PrmtC,
    MXOP_PrmtI,
    MXOP_PrmtR,
    MXOP_PrmtRc,
    MXOP_Pset,
    MXOP_Psetp,
    MXOP_R2b,
    MXOP_R2pC,
    MXOP_R2pI,
    MXOP_R2pR,
    MXOP_Ram,
    MXOP_Red,
    MXOP_Ret,
    MXOP_RroC,
    MXOP_RroI,
    MXOP_RroR,
    MXOP_Rtt,
    MXOP_S2r,
    MXOP_Sam,
    MXOP_SelC,
    MXOP_SelI,
    MXOP_SelR,
    MXOP_Setcrsptr,
    MXOP_Setlmembase,
    MXOP_ShfLI,
    MXOP_ShfLR,
    MXOP_ShfRI,
    MXOP_ShfRR,
    MXOP_Shfl,
    MXOP_ShlC,
    MXOP_ShlI,
    MXOP_ShlR,
    MXOP_ShrC,
    MXOP_ShrI,
    MXOP_ShrR,
    MXOP_Ssy,
    MXOP_St,
    MXOP_Stg,
    MXOP_Stl,
    MXOP_Stp,
    MXOP_Sts,
    MXOP_Suatom,
    MXOP_SuatomB,
    MXOP_SuatomB2,
    MXOP_SuatomCas,
    MXOP_SuatomCasB,
    MXOP_Suld,
    MXOP_SuldB,
    MXOP_SuldD,
    MXOP_SuldDB,
    MXOP_Sured,
    MXOP_SuredB,
    MXOP_Sust,
    MXOP_SustB,
    MXOP_SustD,
    MXOP_SustDB,
    MXOP_Sync,
    MXOP_Tex,
    MXOP_TexB,
    MXOP_Texs,
    MXOP_TexsF16,
    MXOP_Tld,
    MXOP_Tld4,
    MXOP_Tld4B,
    MXOP_Tld4s,
    MXOP_Tld4sF16,
    MXOP_TldB,
    MXOP_Tlds,
    MXOP_TldsF16,
    MXOP_Tmml,
    MXOP_TmmlB,
    MXOP_Txa,
    MXOP_Txd,
    MXOP_TxdB,
    MXOP_Txq,
    MXOP_TxqB,
    MXOP_Vabsdiff,
    MXOP_Vabsdiff4,
    MXOP_Vadd,
    MXOP_Vmad,
    MXOP_Vmnmx,
    MXOP_Vote,
    MXOP_Votevtg,
    MXOP_Vset,
    MXOP_Vsetp,
    MXOP_Vshl,
    MXOP_Vshr,
    MXOP_XmadC,
    MXOP_XmadI,
    MXOP_XmadR,
    MXOP_XmadRc,
    MXOP_COUNT
};
static const char *const mx_op_name[MXOP_COUNT] = {"invalid", "Al2p", "Ald", "Ast", "Atom", "AtomCas", "Atoms", "AtomsCas", "B2r", "Bar", "BfeC", "BfeI", "BfeR", "BfiC", "BfiI", "BfiR", "BfiRc", "Bpt", "Bra", "Brk", "Brx", "Cal", "Cctl", "Cctll", "Cctlt", "Cont", "Cs2r", "Cset", "Csetp", "DaddC", "DaddI", "DaddR", "Depbar", "DfmaC", "DfmaI", "DfmaR", "DfmaRc", "DmnmxC", "DmnmxI", "DmnmxR", "DmulC", "DmulI", "DmulR", "DsetC", "DsetI", "DsetR", "DsetpC", "DsetpI", "DsetpR", "Exit", "F2fC", "F2fI", "F2fR", "F2iC", "F2iI", "F2iR", "Fadd32i", "FaddC", "FaddI", "FaddR", "FchkC", "FchkI", "FchkR", "FcmpC", "FcmpI", "FcmpR", "FcmpRc", "Ffma32i", "FfmaC", "FfmaI", "FfmaR", "FfmaRc", "FloC", "FloI", "FloR", "FmnmxC", "FmnmxI", "FmnmxR", "Fmul32i", "FmulC", "FmulI", "FmulR", "FsetC", "FsetI", "FsetR", "FsetpC", "FsetpI", "FsetpR", "Fswzadd", "Getcrsptr", "Getlmembase", "Hadd232i", "Hadd2C", "Hadd2I", "Hadd2R", "Hfma232i", "Hfma2C", "Hfma2I", "Hfma2R", "Hfma2Rc", "Hmul232i", "Hmul2C", "Hmul2I", "Hmul2R", "Hset2C", "Hset2I", "Hset2R", "Hsetp2C", "Hsetp2I", "Hsetp2R", "I2fC", "I2fI", "I2fR", "I2iC", "I2iI", "I2iR", "Iadd32i", "Iadd3C", "Iadd3I", "Iadd3R", "IaddC", "IaddI", "IaddR", "IcmpC", "IcmpI", "IcmpR", "IcmpRc", "Ide", "IdpC", "IdpR", "Imad32i", "ImadC", "ImadI", "ImadR", "ImadRc", "ImadspC", "ImadspI", "ImadspR", "ImadspRc", "ImnmxC", "ImnmxI", "ImnmxR", "Imul32i", "ImulC", "ImulI", "ImulR", "Ipa", "Isberd", "Iscadd32i", "IscaddC", "IscaddI", "IscaddR", "IsetC", "IsetI", "IsetR", "IsetpC", "IsetpI", "IsetpR", "Jcal", "Jmp", "Jmx", "Kil", "Ld", "Ldc", "Ldg", "Ldl", "Lds", "LeaC", "LeaHiC", "LeaHiR", "LeaI", "LeaR", "Lepc", "Longjmp", "Lop32i", "Lop3C", "Lop3I", "Lop3R", "LopC", "LopI", "LopR", "Membar", "Mov32i", "MovC", "MovI", "MovR", "Mufu", "Nop", "OutC", "OutI", "OutR", "P2rC", "P2rI", "P2rR", "Pbk", "Pcnt", "Pexit", "Pixld", "Plongjmp", "PopcC", "PopcI", "PopcR", "Pret", "PrmtC", "PrmtI", "PrmtR", "PrmtRc", "Pset", "Psetp", "R2b", "R2pC", "R2pI", "R2pR", "Ram", "Red", "Ret", "RroC", "RroI", "RroR", "Rtt", "S2r", "Sam", "SelC", "SelI", "SelR", "Setcrsptr", "Setlmembase", "ShfLI", "ShfLR", "ShfRI", "ShfRR", "Shfl", "ShlC", "ShlI", "ShlR", "ShrC", "ShrI", "ShrR", "Ssy", "St", "Stg", "Stl", "Stp", "Sts", "Suatom", "SuatomB", "SuatomB2", "SuatomCas", "SuatomCasB", "Suld", "SuldB", "SuldD", "SuldDB", "Sured", "SuredB", "Sust", "SustB", "SustD", "SustDB", "Sync", "Tex", "TexB", "Texs", "TexsF16", "Tld", "Tld4", "Tld4B", "Tld4s", "Tld4sF16", "TldB", "Tlds", "TldsF16", "Tmml", "TmmlB", "Txa", "Txd", "TxdB", "Txq", "TxqB", "Vabsdiff", "Vabsdiff4", "Vadd", "Vmad", "Vmnmx", "Vote", "Votevtg", "Vset", "Vsetp", "Vshl", "Vshr", "XmadC", "XmadI", "XmadR", "XmadRc"};

// Las 14 primeras cifras de la instrucción eligen la operación: valor, máscara y cuántas "x" tiene.
typedef struct { uint16_t value, mask; uint8_t xbits; uint16_t op; } MxPattern;
static const MxPattern mx_patterns[] = {
    {0x3be8, 0x3ffe, 1, MXOP_Al2p},
    {0x3bf6, 0x3ffe, 1, MXOP_Ald},
    {0x3bfc, 0x3ffe, 1, MXOP_Ast},
    {0x3b40, 0x3fc0, 6, MXOP_Atom},
    {0x3bbc, 0x3ffc, 2, MXOP_AtomCas},
    {0x3b00, 0x3fc0, 6, MXOP_Atoms},
    {0x3b80, 0x3fe0, 5, MXOP_AtomsCas},
    {0x3c2e, 0x3ffe, 1, MXOP_B2r},
    {0x3c2a, 0x3ffe, 1, MXOP_Bar},
    {0x1700, 0x3ffe, 1, MXOP_BfeR},
    {0x0e00, 0x3fbe, 2, MXOP_BfeI},
    {0x1300, 0x3ffe, 1, MXOP_BfeC},
    {0x16fc, 0x3ffe, 1, MXOP_BfiR},
    {0x0dbc, 0x3fbe, 2, MXOP_BfiI},
    {0x12fc, 0x3ffe, 1, MXOP_BfiC},
    {0x14fc, 0x3ffe, 1, MXOP_BfiRc},
    {0x38e8, 0x3ffc, 2, MXOP_Bpt},
    {0x3890, 0x3ffc, 2, MXOP_Bra},
    {0x38d0, 0x3ffc, 2, MXOP_Brk},
    {0x3894, 0x3ffc, 2, MXOP_Brx},
    {0x3898, 0x3ffc, 2, MXOP_Cal},
    {0x3bd8, 0x3ff8, 3, MXOP_Cctl},
    {0x3be0, 0x3ffe, 1, MXOP_Cctll},
    {0x3afc, 0x3ffe, 1, MXOP_Cctlt},
    {0x3afa, 0x3ffe, 1, MXOP_Cctlt},
    {0x38d4, 0x3ffc, 2, MXOP_Cont},
    {0x1426, 0x3ffe, 1, MXOP_Cset},
    {0x1428, 0x3ffe, 1, MXOP_Csetp},
    {0x1432, 0x3ffe, 1, MXOP_Cs2r},
    {0x171c, 0x3ffe, 1, MXOP_DaddR},
    {0x0e1c, 0x3fbe, 2, MXOP_DaddI},
    {0x131c, 0x3ffe, 1, MXOP_DaddC},
    {0x3c3c, 0x3ffe, 1, MXOP_Depbar},
    {0x16dc, 0x3ffc, 2, MXOP_DfmaR},
    {0x0d9c, 0x3fbc, 3, MXOP_DfmaI},
    {0x12dc, 0x3ffc, 2, MXOP_DfmaC},
    {0x14dc, 0x3ffc, 2, MXOP_DfmaRc},
    {0x1714, 0x3ffe, 1, MXOP_DmnmxR},
    {0x0e14, 0x3fbe, 2, MXOP_DmnmxI},
    {0x1314, 0x3ffe, 1, MXOP_DmnmxC},
    {0x1720, 0x3ffe, 1, MXOP_DmulR},
    {0x0e20, 0x3fbe, 2, MXOP_DmulI},
    {0x1320, 0x3ffe, 1, MXOP_DmulC},
    {0x1640, 0x3fe0, 5, MXOP_DsetR},
    {0x0c80, 0x3fa0, 6, MXOP_DsetI},
    {0x1240, 0x3fe0, 5, MXOP_DsetC},
    {0x16e0, 0x3ffc, 2, MXOP_DsetpR},
    {0x0da0, 0x3fbc, 3, MXOP_DsetpI},
    {0x12e0, 0x3ffc, 2, MXOP_DsetpC},
    {0x38c0, 0x3ffc, 2, MXOP_Exit},
    {0x172a, 0x3ffe, 1, MXOP_F2fR},
    {0x0e2a, 0x3fbe, 2, MXOP_F2fI},
    {0x132a, 0x3ffe, 1, MXOP_F2fC},
    {0x172c, 0x3ffe, 1, MXOP_F2iR},
    {0x0e2c, 0x3fbe, 2, MXOP_F2iI},
    {0x132c, 0x3ffe, 1, MXOP_F2iC},
    {0x1716, 0x3ffe, 1, MXOP_FaddR},
    {0x0e16, 0x3fbe, 2, MXOP_FaddI},
    {0x1316, 0x3ffe, 1, MXOP_FaddC},
    {0x0200, 0x3f00, 8, MXOP_Fadd32i},
    {0x1722, 0x3ffe, 1, MXOP_FchkR},
    {0x0e22, 0x3fbe, 2, MXOP_FchkI},
    {0x1322, 0x3ffe, 1, MXOP_FchkC},
    {0x16e8, 0x3ffc, 2, MXOP_FcmpR},
    {0x0da8, 0x3fbc, 3, MXOP_FcmpI},
    {0x12e8, 0x3ffc, 2, MXOP_FcmpC},
    {0x14e8, 0x3ffc, 2, MXOP_FcmpRc},
    {0x1660, 0x3fe0, 5, MXOP_FfmaR},
    {0x0ca0, 0x3fa0, 6, MXOP_FfmaI},
    {0x1260, 0x3fe0, 5, MXOP_FfmaC},
    {0x1460, 0x3fe0, 5, MXOP_FfmaRc},
    {0x0300, 0x3f00, 8, MXOP_Ffma32i},
    {0x170c, 0x3ffe, 1, MXOP_FloR},
    {0x0e0c, 0x3fbe, 2, MXOP_FloI},
    {0x130c, 0x3ffe, 1, MXOP_FloC},
    {0x1718, 0x3ffe, 1, MXOP_FmnmxR},
    {0x0e18, 0x3fbe, 2, MXOP_FmnmxI},
    {0x1318, 0x3ffe, 1, MXOP_FmnmxC},
    {0x171a, 0x3ffe, 1, MXOP_FmulR},
    {0x0e1a, 0x3fbe, 2, MXOP_FmulI},
    {0x131a, 0x3ffe, 1, MXOP_FmulC},
    {0x0780, 0x3fc0, 6, MXOP_Fmul32i},
    {0x1600, 0x3fc0, 6, MXOP_FsetR},
    {0x0c00, 0x3f80, 7, MXOP_FsetI},
    {0x1200, 0x3fc0, 6, MXOP_FsetC},
    {0x16ec, 0x3ffc, 2, MXOP_FsetpR},
    {0x0dac, 0x3fbc, 3, MXOP_FsetpI},
    {0x12ec, 0x3ffc, 2, MXOP_FsetpC},
    {0x143e, 0x3ffe, 1, MXOP_Fswzadd},
    {0x38b0, 0x3ffc, 2, MXOP_Getcrsptr},
    {0x38b4, 0x3ffc, 2, MXOP_Getlmembase},
    {0x1744, 0x3ffe, 1, MXOP_Hadd2R},
    {0x1e80, 0x3fa0, 6, MXOP_Hadd2I},
    {0x1ea0, 0x3fa0, 6, MXOP_Hadd2C},
    {0x0b00, 0x3f80, 7, MXOP_Hadd232i},
    {0x1740, 0x3ffe, 1, MXOP_Hfma2R},
    {0x1c00, 0x3e20, 8, MXOP_Hfma2I},
    {0x1c20, 0x3e20, 8, MXOP_Hfma2C},
    {0x1820, 0x3e20, 8, MXOP_Hfma2Rc},
    {0x0a00, 0x3f80, 7, MXOP_Hfma232i},
    {0x1742, 0x3ffe, 1, MXOP_Hmul2R},
    {0x1e00, 0x3fa0, 6, MXOP_Hmul2I},
    {0x1e20, 0x3fa0, 6, MXOP_Hmul2C},
    {0x0a80, 0x3f80, 7, MXOP_Hmul232i},
    {0x1746, 0x3ffe, 1, MXOP_Hset2R},
    {0x1f00, 0x3fa0, 6, MXOP_Hset2I},
    {0x1f20, 0x3fa0, 6, MXOP_Hset2C},
    {0x1748, 0x3ffe, 1, MXOP_Hsetp2R},
    {0x1f80, 0x3fa0, 6, MXOP_Hsetp2I},
    {0x1fa0, 0x3fa0, 6, MXOP_Hsetp2C},
    {0x172e, 0x3ffe, 1, MXOP_I2fR},
    {0x0e2e, 0x3fbe, 2, MXOP_I2fI},
    {0x132e, 0x3ffe, 1, MXOP_I2fC},
    {0x1738, 0x3ffe, 1, MXOP_I2iR},
    {0x0e38, 0x3fbe, 2, MXOP_I2iI},
    {0x1338, 0x3ffe, 1, MXOP_I2iC},
    {0x1704, 0x3ffe, 1, MXOP_IaddR},
    {0x0e04, 0x3fbe, 2, MXOP_IaddI},
    {0x1304, 0x3ffe, 1, MXOP_IaddC},
    {0x0700, 0x3f80, 7, MXOP_Iadd32i},
    {0x1730, 0x3ffc, 2, MXOP_Iadd3R},
    {0x0e30, 0x3fbc, 3, MXOP_Iadd3I},
    {0x1330, 0x3ffc, 2, MXOP_Iadd3C},
    {0x16d0, 0x3ffc, 2, MXOP_IcmpR},
    {0x0d90, 0x3fbc, 3, MXOP_IcmpI},
    {0x12d0, 0x3ffc, 2, MXOP_IcmpC},
    {0x14d0, 0x3ffc, 2, MXOP_IcmpRc},
    {0x38e4, 0x3ffc, 2, MXOP_Ide},
    {0x14fe, 0x3ffe, 1, MXOP_IdpR},
    {0x14f6, 0x3ffe, 1, MXOP_IdpC},
    {0x1680, 0x3fe0, 5, MXOP_ImadR},
    {0x0d00, 0x3fa0, 6, MXOP_ImadI},
    {0x1280, 0x3fe0, 5, MXOP_ImadC},
    {0x1480, 0x3fe0, 5, MXOP_ImadRc},
    {0x0400, 0x3f00, 8, MXOP_Imad32i},
    {0x16a0, 0x3fe0, 5, MXOP_ImadspR},
    {0x0d20, 0x3fa0, 6, MXOP_ImadspI},
    {0x12a0, 0x3fe0, 5, MXOP_ImadspC},
    {0x14a0, 0x3fe0, 5, MXOP_ImadspRc},
    {0x1708, 0x3ffe, 1, MXOP_ImnmxR},
    {0x0e08, 0x3fbe, 2, MXOP_ImnmxI},
    {0x1308, 0x3ffe, 1, MXOP_ImnmxC},
    {0x170e, 0x3ffe, 1, MXOP_ImulR},
    {0x0e0e, 0x3fbe, 2, MXOP_ImulI},
    {0x130e, 0x3ffe, 1, MXOP_ImulC},
    {0x07c0, 0x3fc0, 6, MXOP_Imul32i},
    {0x3800, 0x3fc0, 6, MXOP_Ipa},
    {0x3bf4, 0x3ffe, 1, MXOP_Isberd},
    {0x1706, 0x3ffe, 1, MXOP_IscaddR},
    {0x0e06, 0x3fbe, 2, MXOP_IscaddI},
    {0x1306, 0x3ffe, 1, MXOP_IscaddC},
    {0x0500, 0x3f00, 8, MXOP_Iscadd32i},
    {0x16d4, 0x3ffc, 2, MXOP_IsetR},
    {0x0d94, 0x3fbc, 3, MXOP_IsetI},
    {0x12d4, 0x3ffc, 2, MXOP_IsetC},
    {0x16d8, 0x3ffc, 2, MXOP_IsetpR},
    {0x0d98, 0x3fbc, 3, MXOP_IsetpI},
    {0x12d8, 0x3ffc, 2, MXOP_IsetpC},
    {0x3888, 0x3ffc, 2, MXOP_Jcal},
    {0x3884, 0x3ffc, 2, MXOP_Jmp},
    {0x3880, 0x3ffc, 2, MXOP_Jmx},
    {0x38cc, 0x3ffc, 2, MXOP_Kil},
    {0x2000, 0x3800, 11, MXOP_Ld},
    {0x3be4, 0x3ffe, 1, MXOP_Ldc},
    {0x3bb4, 0x3ffe, 1, MXOP_Ldg},
    {0x3bd0, 0x3ffe, 1, MXOP_Ldl},
    {0x3bd2, 0x3ffe, 1, MXOP_Lds},
    {0x16f4, 0x3ffe, 1, MXOP_LeaR},
    {0x0db4, 0x3fbe, 2, MXOP_LeaI},
    {0x12f4, 0x3ffe, 1, MXOP_LeaC},
    {0x16f6, 0x3ffe, 1, MXOP_LeaHiR},
    {0x0600, 0x3f00, 8, MXOP_LeaHiC},
    {0x1434, 0x3ffe, 1, MXOP_Lepc},
    {0x38c4, 0x3ffc, 2, MXOP_Longjmp},
    {0x1710, 0x3ffe, 1, MXOP_LopR},
    {0x0e10, 0x3fbe, 2, MXOP_LopI},
    {0x1310, 0x3ffe, 1, MXOP_LopC},
    {0x16f8, 0x3ffe, 1, MXOP_Lop3R},
    {0x0f00, 0x3f00, 8, MXOP_Lop3I},
    {0x0080, 0x3f80, 7, MXOP_Lop3C},
    {0x0100, 0x3f00, 8, MXOP_Lop32i},
    {0x3be6, 0x3ffe, 1, MXOP_Membar},
    {0x1726, 0x3ffe, 1, MXOP_MovR},
    {0x0e26, 0x3fbe, 2, MXOP_MovI},
    {0x1326, 0x3ffe, 1, MXOP_MovC},
    {0x0040, 0x3ffc, 2, MXOP_Mov32i},
    {0x1420, 0x3ffe, 1, MXOP_Mufu},
    {0x142c, 0x3ffe, 1, MXOP_Nop},
    {0x3ef8, 0x3ffe, 1, MXOP_OutR},
    {0x3db8, 0x3fbe, 2, MXOP_OutI},
    {0x3af8, 0x3ffe, 1, MXOP_OutC},
    {0x173a, 0x3ffe, 1, MXOP_P2rR},
    {0x0e3a, 0x3fbe, 2, MXOP_P2rI},
    {0x133a, 0x3ffe, 1, MXOP_P2rC},
    {0x38a8, 0x3ffc, 2, MXOP_Pbk},
    {0x38ac, 0x3ffc, 2, MXOP_Pcnt},
    {0x388c, 0x3ffc, 2, MXOP_Pexit},
    {0x3bfa, 0x3ffe, 1, MXOP_Pixld},
    {0x38a0, 0x3ffc, 2, MXOP_Plongjmp},
    {0x1702, 0x3ffe, 1, MXOP_PopcR},
    {0x0e02, 0x3fbe, 2, MXOP_PopcI},
    {0x1302, 0x3ffe, 1, MXOP_PopcC},
    {0x389c, 0x3ffc, 2, MXOP_Pret},
    {0x16f0, 0x3ffc, 2, MXOP_PrmtR},
    {0x0db0, 0x3fbc, 3, MXOP_PrmtI},
    {0x12f0, 0x3ffc, 2, MXOP_PrmtC},
    {0x14f0, 0x3ffc, 2, MXOP_PrmtRc},
    {0x1422, 0x3ffe, 1, MXOP_Pset},
    {0x1424, 0x3ffe, 1, MXOP_Psetp},
    {0x3c30, 0x3ffe, 1, MXOP_R2b},
    {0x173c, 0x3ffe, 1, MXOP_R2pR},
    {0x0e3c, 0x3fbe, 2, MXOP_R2pI},
    {0x133c, 0x3ffe, 1, MXOP_R2pC},
    {0x38e0, 0x3ffc, 2, MXOP_Ram},
    {0x3afe, 0x3ffe, 1, MXOP_Red},
    {0x38c8, 0x3ffc, 2, MXOP_Ret},
    {0x1724, 0x3ffe, 1, MXOP_RroR},
    {0x0e24, 0x3fbe, 2, MXOP_RroI},
    {0x1324, 0x3ffe, 1, MXOP_RroC},
    {0x38d8, 0x3ffc, 2, MXOP_Rtt},
    {0x3c32, 0x3ffe, 1, MXOP_S2r},
    {0x38dc, 0x3ffc, 2, MXOP_Sam},
    {0x1728, 0x3ffe, 1, MXOP_SelR},
    {0x0e28, 0x3fbe, 2, MXOP_SelI},
    {0x1328, 0x3ffe, 1, MXOP_SelC},
    {0x38b8, 0x3ffc, 2, MXOP_Setcrsptr},
    {0x38bc, 0x3ffc, 2, MXOP_Setlmembase},
    {0x16fe, 0x3ffe, 1, MXOP_ShfLR},
    {0x173e, 0x3ffe, 1, MXOP_ShfRR},
    {0x0dbe, 0x3fbe, 2, MXOP_ShfLI},
    {0x0e3e, 0x3fbe, 2, MXOP_ShfRI},
    {0x3bc4, 0x3ffe, 1, MXOP_Shfl},
    {0x1712, 0x3ffe, 1, MXOP_ShlR},
    {0x0e12, 0x3fbe, 2, MXOP_ShlI},
    {0x1312, 0x3ffe, 1, MXOP_ShlC},
    {0x170a, 0x3ffe, 1, MXOP_ShrR},
    {0x0e0a, 0x3fbe, 2, MXOP_ShrI},
    {0x130a, 0x3ffe, 1, MXOP_ShrC},
    {0x38a4, 0x3ffc, 2, MXOP_Ssy},
    {0x2800, 0x3800, 11, MXOP_St},
    {0x3bb6, 0x3ffe, 1, MXOP_Stg},
    {0x3bd4, 0x3ffe, 1, MXOP_Stl},
    {0x3ba8, 0x3ffe, 1, MXOP_Stp},
    {0x3bd6, 0x3ffe, 1, MXOP_Sts},
    {0x3a9c, 0x3ffe, 1, MXOP_SuatomB},
    {0x3a80, 0x3fd0, 5, MXOP_Suatom},
    {0x3ae0, 0x3ff0, 4, MXOP_SuatomB2},
    {0x3ab4, 0x3ffe, 1, MXOP_SuatomCasB},
    {0x3aa0, 0x3fa0, 6, MXOP_SuatomCas},
    {0x3ac4, 0x3ffe, 1, MXOP_SuldDB},
    {0x3ac6, 0x3ffe, 1, MXOP_SuldD},
    {0x3ac0, 0x3ffe, 1, MXOP_SuldB},
    {0x3ac2, 0x3ffe, 1, MXOP_Suld},
    {0x3ad4, 0x3ffe, 1, MXOP_SuredB},
    {0x3ad6, 0x3ffe, 1, MXOP_Sured},
    {0x3acc, 0x3ffe, 1, MXOP_SustDB},
    {0x3ace, 0x3ffe, 1, MXOP_SustD},
    {0x3ac8, 0x3ffe, 1, MXOP_SustB},
    {0x3aca, 0x3ffe, 1, MXOP_Sust},
    {0x3c3e, 0x3ffe, 1, MXOP_Sync},
    {0x3000, 0x3e00, 9, MXOP_Tex},
    {0x37a0, 0x3ff0, 4, MXOP_TexB},
    {0x3600, 0x3f80, 7, MXOP_Texs},
    {0x3400, 0x3f80, 7, MXOP_TexsF16},
    {0x3700, 0x3fc0, 6, MXOP_Tld},
    {0x3740, 0x3fc0, 6, MXOP_TldB},
    {0x3680, 0x3f80, 7, MXOP_Tlds},
    {0x3480, 0x3f80, 7, MXOP_TldsF16},
    {0x3200, 0x3f00, 8, MXOP_Tld4},
    {0x37b0, 0x3ff0, 4, MXOP_Tld4B},
    {0x37c0, 0x3ff0, 4, MXOP_Tld4s},
    {0x37e0, 0x3ff0, 4, MXOP_Tld4sF16},
    {0x37d6, 0x3ffe, 1, MXOP_Tmml},
    {0x37d8, 0x3ffe, 1, MXOP_TmmlB},
    {0x37d0, 0x3ffe, 1, MXOP_Txa},
    {0x3780, 0x3fe0, 5, MXOP_Txd},
    {0x3790, 0x3ff0, 4, MXOP_TxdB},
    {0x37d2, 0x3ffe, 1, MXOP_Txq},
    {0x37d4, 0x3ffe, 1, MXOP_TxqB},
    {0x1500, 0x3fc0, 6, MXOP_Vabsdiff},
    {0x1400, 0x3fe0, 5, MXOP_Vabsdiff4},
    {0x0800, 0x3f00, 8, MXOP_Vadd},
    {0x17c0, 0x3fc0, 6, MXOP_Vmad},
    {0x0e80, 0x3f80, 7, MXOP_Vmnmx},
    {0x1436, 0x3ffe, 1, MXOP_Vote},
    {0x1438, 0x3ffe, 1, MXOP_Votevtg},
    {0x1000, 0x3f80, 7, MXOP_Vset},
    {0x143c, 0x3ffe, 1, MXOP_Vsetp},
    {0x15c0, 0x3fc0, 6, MXOP_Vshl},
    {0x1580, 0x3fc0, 6, MXOP_Vshr},
    {0x16c0, 0x3ff0, 4, MXOP_XmadR},
    {0x0d80, 0x3fb0, 5, MXOP_XmadI},
    {0x1380, 0x3f80, 7, MXOP_XmadC},
    {0x1440, 0x3fe0, 5, MXOP_XmadRc},
};

#define Al2p_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Al2p_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Al2p_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Al2p_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Al2p_AlSize(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Al2p_Aio(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Al2p_Imm11(o) ((int64_t)((((o) >> 20) & 0x7FF)))
#define Al2p_DestPred(o) ((int64_t)((((o) >> 44) & 0x7)))
#define Ald_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ald_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ald_SrcB(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Ald_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ald_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ald_Imm11(o) ((int64_t)((((o) >> 20) & 0x7FF)))
#define Ald_P(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Ald_O(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Ald_AlSize(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Ast_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ast_SrcB(o) ((int64_t)(((o) & 0xFF)))
#define Ast_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Ast_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ast_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ast_Imm11(o) ((int64_t)((((o) >> 20) & 0x7FF)))
#define Ast_P(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Ast_AlSize(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Atom_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Atom_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Atom_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Atom_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Atom_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Atom_Imm20(o) ((int64_t)((((o) >> 28) & 0xFFFFF)))
#define Atom_Size(o) ((int64_t)((((o) >> 49) & 0x7)))
#define Atom_Op(o) ((int64_t)((((o) >> 52) & 0xF)))
#define Atom_E(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define AtomCas_Dest(o) ((int64_t)(((o) & 0xFF)))
#define AtomCas_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define AtomCas_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define AtomCas_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define AtomCas_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define AtomCas_BcRz(o) ((int64_t)((((o) >> 50) & 0x3)))
#define AtomCas_E(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Atoms_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Atoms_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Atoms_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Atoms_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Atoms_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Atoms_Imm22(o) ((int64_t)((((o) >> 30) & 0x3FFFFF)))
#define Atoms_AtomsSize(o) ((int64_t)((((o) >> 28) & 0x3)))
#define Atoms_AtomOp(o) ((int64_t)((((o) >> 52) & 0xF)))
#define AtomsCas_Dest(o) ((int64_t)(((o) & 0xFF)))
#define AtomsCas_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define AtomsCas_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define AtomsCas_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define AtomsCas_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define AtomsCas_AtomsBcRz(o) ((int64_t)((((o) >> 28) & 0x3)))
#define B2r_Dest(o) ((int64_t)(((o) & 0xFF)))
#define B2r_DestPred(o) ((int64_t)((((o) >> 45) & 0x7)))
#define B2r_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define B2r_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define B2r_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define B2r_Mode(o) ((int64_t)((((o) >> 32) & 0x3)))
#define Bar_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Bar_Imm12(o) ((int64_t)((((o) >> 20) & 0xFFF)))
#define Bar_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Bar_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Bar_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Bar_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Bar_BarOp(o) ((int64_t)((((o) >> 32) & 0x7)))
#define Bar_BarRedOp(o) ((int64_t)((((o) >> 35) & 0x3)))
#define Bar_AFixBar(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Bar_BFixBar(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define BfeC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfeC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfeC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define BfeC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define BfeC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfeC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfeC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define BfeC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define BfeC_Brev(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define BfeI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfeI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfeI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define BfeI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfeI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfeI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define BfeI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define BfeI_Brev(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define BfeR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfeR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfeR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define BfeR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfeR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfeR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define BfeR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define BfeR_Brev(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define BfiC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfiC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfiC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define BfiC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define BfiC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define BfiC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfiC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfiC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define BfiI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfiI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfiI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define BfiI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define BfiI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfiI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfiI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define BfiR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfiR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfiR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define BfiR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define BfiR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfiR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfiR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define BfiRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define BfiRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define BfiRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define BfiRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define BfiRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define BfiRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define BfiRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define BfiRc_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Bpt_Imm20(o) ((int64_t)((((o) >> 20) & 0xFFFFF)))
#define Bpt_Bpt(o) ((int64_t)((((o) >> 6) & 0x7)))
#define Bra_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Bra_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Bra_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Bra_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Bra_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Bra_Lmt(o) ((int64_t)(((o) & 0x40) != 0))
#define Bra_U(o) ((int64_t)(((o) & 0x80) != 0))
#define Brk_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Brk_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Brk_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Brx_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Brx_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Brx_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Brx_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Brx_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Brx_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Brx_Lmt(o) ((int64_t)(((o) & 0x40) != 0))
#define Cal_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Cal_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Cal_Inc(o) ((int64_t)(((o) & 0x40) != 0))
#define Cctl_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Cctl_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Cctl_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Cctl_Imm30(o) ((int64_t)((((o) >> 22) & 0x3FFFFFFF)))
#define Cctl_E(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Cctl_Cache(o) ((int64_t)((((o) >> 4) & 0x7)))
#define Cctl_CctlOp(o) ((int64_t)(((o) & 0xF)))
#define Cctll_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Cctll_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Cctll_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Cctll_Imm22(o) ((int64_t)((((o) >> 22) & 0x3FFFFF)))
#define Cctll_Cache(o) ((int64_t)((((o) >> 4) & 0x3)))
#define Cctll_CctlOp(o) ((int64_t)(((o) & 0xF)))
#define Cctlt_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Cctlt_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Cctlt_TsIdx13(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Cctlt_CctltOp(o) ((int64_t)(((o) & 0x3)))
#define Cont_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Cont_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Cont_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Cs2r_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Cs2r_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Cs2r_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Cs2r_SReg(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Cset_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Cset_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Cset_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Cset_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Cset_Ccc(o) ((int64_t)((((o) >> 8) & 0x1F)))
#define Cset_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Cset_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Cset_BVal(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Cset_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Csetp_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Csetp_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Csetp_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Csetp_Ccc(o) ((int64_t)((((o) >> 8) & 0x1F)))
#define Csetp_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define Csetp_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Csetp_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Csetp_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Csetp_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DaddC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DaddC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DaddC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DaddC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DaddC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DaddC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DaddC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DaddC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DaddC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DaddC_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define DaddC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define DaddC_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define DaddI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DaddI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DaddI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define DaddI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DaddI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DaddI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DaddI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DaddI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DaddI_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define DaddI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define DaddI_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define DaddR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DaddR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DaddR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define DaddR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DaddR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DaddR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DaddR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DaddR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DaddR_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define DaddR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define DaddR_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define Depbar_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Depbar_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Depbar_Le(o) ((int64_t)(((o) & 0x20000000) != 0))
#define Depbar_Sbid(o) ((int64_t)((((o) >> 26) & 0x7)))
#define Depbar_PendCnt(o) ((int64_t)((((o) >> 20) & 0x3F)))
#define Depbar_Imm6(o) ((int64_t)(((o) & 0x3F)))
#define DfmaC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DfmaC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DfmaC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DfmaC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DfmaC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define DfmaC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DfmaC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DfmaC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DfmaC_RoundMode(o) ((int64_t)((((o) >> 50) & 0x3)))
#define DfmaC_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DfmaC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DfmaI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DfmaI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DfmaI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define DfmaI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define DfmaI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DfmaI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DfmaI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DfmaI_RoundMode(o) ((int64_t)((((o) >> 50) & 0x3)))
#define DfmaI_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DfmaI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DfmaR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DfmaR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DfmaR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define DfmaR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define DfmaR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DfmaR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DfmaR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DfmaR_RoundMode(o) ((int64_t)((((o) >> 50) & 0x3)))
#define DfmaR_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DfmaR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DfmaRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DfmaRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DfmaRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define DfmaRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DfmaRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DfmaRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DfmaRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DfmaRc_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DfmaRc_RoundMode(o) ((int64_t)((((o) >> 50) & 0x3)))
#define DfmaRc_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DfmaRc_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DmnmxC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DmnmxC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DmnmxC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DmnmxC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DmnmxC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DmnmxC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DmnmxC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DmnmxC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DmnmxC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DmnmxC_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define DmnmxC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define DmnmxC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DmnmxC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DmnmxI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DmnmxI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DmnmxI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define DmnmxI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DmnmxI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DmnmxI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DmnmxI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DmnmxI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DmnmxI_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define DmnmxI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define DmnmxI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DmnmxI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DmnmxR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DmnmxR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DmnmxR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define DmnmxR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DmnmxR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DmnmxR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DmnmxR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define DmnmxR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DmnmxR_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define DmnmxR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define DmnmxR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DmnmxR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DmulC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DmulC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DmulC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DmulC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DmulC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DmulC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DmulC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DmulC_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define DmulC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DmulI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DmulI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DmulI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define DmulI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DmulI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DmulI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DmulI_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define DmulI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DmulR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DmulR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DmulR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define DmulR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DmulR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DmulR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DmulR_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define DmulR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define DsetC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DsetC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DsetC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DsetC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DsetC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DsetC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DsetC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DsetC_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define DsetC_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define DsetC_BVal(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define DsetC_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define DsetC_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DsetC_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define DsetC_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define DsetC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DsetC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DsetI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DsetI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DsetI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define DsetI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DsetI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DsetI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DsetI_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define DsetI_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define DsetI_BVal(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define DsetI_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define DsetI_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DsetI_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define DsetI_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define DsetI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DsetI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DsetR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define DsetR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DsetR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define DsetR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DsetR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DsetR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define DsetR_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define DsetR_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define DsetR_BVal(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define DsetR_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define DsetR_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DsetR_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define DsetR_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define DsetR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DsetR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DsetpC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DsetpC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define DsetpC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define DsetpC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DsetpC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DsetpC_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define DsetpC_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DsetpC_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define DsetpC_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define DsetpC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DsetpC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DsetpC_AbsA(o) ((int64_t)(((o) & 0x80) != 0))
#define DsetpC_NegB(o) ((int64_t)(((o) & 0x40) != 0))
#define DsetpC_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define DsetpC_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define DsetpI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DsetpI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define DsetpI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DsetpI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DsetpI_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define DsetpI_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DsetpI_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define DsetpI_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define DsetpI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DsetpI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DsetpI_AbsA(o) ((int64_t)(((o) & 0x80) != 0))
#define DsetpI_NegB(o) ((int64_t)(((o) & 0x40) != 0))
#define DsetpI_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define DsetpI_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define DsetpR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define DsetpR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define DsetpR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define DsetpR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define DsetpR_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define DsetpR_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define DsetpR_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define DsetpR_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define DsetpR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define DsetpR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define DsetpR_AbsA(o) ((int64_t)(((o) & 0x80) != 0))
#define DsetpR_NegB(o) ((int64_t)(((o) & 0x40) != 0))
#define DsetpR_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define DsetpR_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Exit_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Exit_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Exit_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Exit_KeepRefCnt(o) ((int64_t)(((o) & 0x20) != 0))
#define F2fC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define F2fC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define F2fC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define F2fC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define F2fC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define F2fC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define F2fC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define F2fC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define F2fC_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define F2fC_DstFmt(o) ((int64_t)((((o) >> 8) & 0x3)))
#define F2fC_SrcFmt(o) ((int64_t)((((o) >> 10) & 0x3)))
#define F2fC_RoundMode(o) ((int64_t)(((((o) >> 40) & 0x4) | (((o) >> 39) & 0x3))))
#define F2fC_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define F2fC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define F2fI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define F2fI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define F2fI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define F2fI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define F2fI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define F2fI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define F2fI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define F2fI_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define F2fI_DstFmt(o) ((int64_t)((((o) >> 8) & 0x3)))
#define F2fI_SrcFmt(o) ((int64_t)((((o) >> 10) & 0x3)))
#define F2fI_RoundMode(o) ((int64_t)(((((o) >> 40) & 0x4) | (((o) >> 39) & 0x3))))
#define F2fI_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define F2fI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define F2fR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define F2fR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define F2fR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define F2fR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define F2fR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define F2fR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define F2fR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define F2fR_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define F2fR_DstFmt(o) ((int64_t)((((o) >> 8) & 0x3)))
#define F2fR_SrcFmt(o) ((int64_t)((((o) >> 10) & 0x3)))
#define F2fR_RoundMode(o) ((int64_t)(((((o) >> 40) & 0x4) | (((o) >> 39) & 0x3))))
#define F2fR_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define F2fR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define F2iC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define F2iC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define F2iC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define F2iC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define F2iC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define F2iC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define F2iC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define F2iC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define F2iC_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define F2iC_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define F2iC_IDstFmt(o) ((int64_t)(((((o) >> 10) & 0x4) | (((o) >> 8) & 0x3))))
#define F2iC_SrcFmt(o) ((int64_t)((((o) >> 10) & 0x3)))
#define F2iC_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define F2iI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define F2iI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define F2iI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define F2iI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define F2iI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define F2iI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define F2iI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define F2iI_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define F2iI_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define F2iI_IDstFmt(o) ((int64_t)(((((o) >> 10) & 0x4) | (((o) >> 8) & 0x3))))
#define F2iI_SrcFmt(o) ((int64_t)((((o) >> 10) & 0x3)))
#define F2iI_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define F2iR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define F2iR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define F2iR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define F2iR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define F2iR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define F2iR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define F2iR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define F2iR_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define F2iR_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define F2iR_IDstFmt(o) ((int64_t)(((((o) >> 10) & 0x4) | (((o) >> 8) & 0x3))))
#define F2iR_SrcFmt(o) ((int64_t)((((o) >> 10) & 0x3)))
#define F2iR_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define Fadd32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Fadd32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Fadd32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Fadd32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Fadd32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Fadd32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Fadd32i_AbsB(o) ((int64_t)(((o) & 0x200000000000000) != 0))
#define Fadd32i_NegA(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Fadd32i_Ftz(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Fadd32i_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Fadd32i_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define FaddC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FaddC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FaddC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FaddC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FaddC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FaddC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FaddC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FaddC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FaddC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FaddC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FaddC_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FaddC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FaddC_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FaddC_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define FaddI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FaddI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FaddI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FaddI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FaddI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FaddI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FaddI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FaddI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FaddI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FaddI_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FaddI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FaddI_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FaddI_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define FaddR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FaddR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FaddR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FaddR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FaddR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FaddR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FaddR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FaddR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FaddR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FaddR_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FaddR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FaddR_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FaddR_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define FchkC_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define FchkC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FchkC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FchkC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FchkC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FchkC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FchkC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FchkC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FchkC_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FchkC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FchkC_ChkModeF(o) ((int64_t)((((o) >> 39) & 0x3F)))
#define FchkI_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define FchkI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FchkI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FchkI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FchkI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FchkI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FchkI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FchkI_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FchkI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FchkI_ChkModeF(o) ((int64_t)((((o) >> 39) & 0x3F)))
#define FchkR_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define FchkR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FchkR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FchkR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FchkR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FchkR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FchkR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FchkR_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FchkR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FchkR_ChkModeF(o) ((int64_t)((((o) >> 39) & 0x3F)))
#define FcmpC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FcmpC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FcmpC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FcmpC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FcmpC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FcmpC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FcmpC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FcmpC_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FcmpC_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FcmpI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FcmpI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FcmpI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FcmpI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FcmpI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FcmpI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FcmpI_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FcmpI_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FcmpR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FcmpR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FcmpR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FcmpR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FcmpR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FcmpR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FcmpR_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FcmpR_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FcmpRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FcmpRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FcmpRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FcmpRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FcmpRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FcmpRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FcmpRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FcmpRc_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FcmpRc_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Ffma32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ffma32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ffma32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Ffma32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ffma32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ffma32i_NegC(o) ((int64_t)(((o) & 0x200000000000000) != 0))
#define Ffma32i_NegA(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Ffma32i_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Ffma32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Ffma32i_Fmz(o) ((int64_t)((((o) >> 53) & 0x3)))
#define FfmaC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FfmaC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FfmaC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FfmaC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FfmaC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FfmaC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FfmaC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FfmaC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FfmaC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FfmaC_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FfmaC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FfmaC_RoundMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define FfmaC_Fmz(o) ((int64_t)((((o) >> 53) & 0x3)))
#define FfmaI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FfmaI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FfmaI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FfmaI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FfmaI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FfmaI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FfmaI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FfmaI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FfmaI_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FfmaI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FfmaI_RoundMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define FfmaI_Fmz(o) ((int64_t)((((o) >> 53) & 0x3)))
#define FfmaR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FfmaR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FfmaR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FfmaR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FfmaR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FfmaR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FfmaR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FfmaR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FfmaR_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FfmaR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FfmaR_RoundMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define FfmaR_Fmz(o) ((int64_t)((((o) >> 53) & 0x3)))
#define FfmaRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FfmaRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FfmaRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define FfmaRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FfmaRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FfmaRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FfmaRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FfmaRc_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FfmaRc_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FfmaRc_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FfmaRc_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FfmaRc_RoundMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define FfmaRc_Fmz(o) ((int64_t)((((o) >> 53) & 0x3)))
#define FloC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FloC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FloC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FloC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FloC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FloC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FloC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FloC_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define FloC_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define FloI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FloI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FloI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FloI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FloI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FloI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FloI_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define FloI_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define FloR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FloR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FloR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FloR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FloR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FloR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FloR_Sh(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define FloR_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define FmnmxC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FmnmxC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FmnmxC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FmnmxC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FmnmxC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FmnmxC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FmnmxC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FmnmxC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FmnmxC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FmnmxC_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FmnmxC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FmnmxC_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FmnmxC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FmnmxC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FmnmxI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FmnmxI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FmnmxI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FmnmxI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FmnmxI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FmnmxI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FmnmxI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FmnmxI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FmnmxI_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FmnmxI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FmnmxI_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FmnmxI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FmnmxI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FmnmxR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FmnmxR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FmnmxR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FmnmxR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FmnmxR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FmnmxR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FmnmxR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define FmnmxR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FmnmxR_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define FmnmxR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define FmnmxR_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FmnmxR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FmnmxR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Fmul32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Fmul32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Fmul32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Fmul32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Fmul32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Fmul32i_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Fmul32i_Fmz(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Fmul32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define FmulC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FmulC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FmulC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FmulC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FmulC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FmulC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FmulC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FmulC_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define FmulC_Fmz(o) ((int64_t)((((o) >> 44) & 0x3)))
#define FmulC_Scale(o) ((int64_t)((((o) >> 41) & 0x7)))
#define FmulC_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FmulC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FmulI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FmulI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FmulI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FmulI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FmulI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FmulI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FmulI_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define FmulI_Fmz(o) ((int64_t)((((o) >> 44) & 0x3)))
#define FmulI_Scale(o) ((int64_t)((((o) >> 41) & 0x7)))
#define FmulI_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FmulI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FmulR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FmulR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FmulR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FmulR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FmulR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FmulR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FmulR_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define FmulR_Fmz(o) ((int64_t)((((o) >> 44) & 0x3)))
#define FmulR_Scale(o) ((int64_t)((((o) >> 41) & 0x7)))
#define FmulR_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define FmulR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define FsetC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FsetC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FsetC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FsetC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FsetC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FsetC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FsetC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetC_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define FsetC_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define FsetC_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define FsetC_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FsetC_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FsetC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FsetC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FsetC_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define FsetC_Ftz(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define FsetC_BVal(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define FsetI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FsetI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FsetI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FsetI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FsetI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FsetI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetI_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define FsetI_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define FsetI_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define FsetI_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FsetI_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FsetI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FsetI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FsetI_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define FsetI_Ftz(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define FsetI_BVal(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define FsetR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define FsetR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FsetR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FsetR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FsetR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FsetR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetR_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define FsetR_NegB(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define FsetR_AbsA(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define FsetR_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FsetR_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FsetR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FsetR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FsetR_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define FsetR_Ftz(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define FsetR_BVal(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define FsetpC_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define FsetpC_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define FsetpC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FsetpC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define FsetpC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define FsetpC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FsetpC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FsetpC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetpC_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define FsetpC_NegB(o) ((int64_t)(((o) & 0x40) != 0))
#define FsetpC_AbsA(o) ((int64_t)(((o) & 0x80) != 0))
#define FsetpC_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FsetpC_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FsetpC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FsetpC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FsetpC_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define FsetpC_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetpI_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define FsetpI_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define FsetpI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FsetpI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define FsetpI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FsetpI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FsetpI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetpI_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define FsetpI_NegB(o) ((int64_t)(((o) & 0x40) != 0))
#define FsetpI_AbsA(o) ((int64_t)(((o) & 0x80) != 0))
#define FsetpI_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FsetpI_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FsetpI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FsetpI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FsetpI_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define FsetpI_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetpR_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define FsetpR_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define FsetpR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define FsetpR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define FsetpR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define FsetpR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define FsetpR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define FsetpR_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define FsetpR_NegB(o) ((int64_t)(((o) & 0x40) != 0))
#define FsetpR_AbsA(o) ((int64_t)(((o) & 0x80) != 0))
#define FsetpR_AbsB(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define FsetpR_FComp(o) ((int64_t)((((o) >> 48) & 0xF)))
#define FsetpR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define FsetpR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define FsetpR_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define FsetpR_Ftz(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Fswzadd_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Fswzadd_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Fswzadd_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Fswzadd_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Fswzadd_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Fswzadd_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Fswzadd_Ftz(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Fswzadd_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define Fswzadd_Ndv(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define Fswzadd_PnWord(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define Getcrsptr_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Getlmembase_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hadd232i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hadd232i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hadd232i_Imm(o) ((int64_t)(((o) >> 20)))
#define Hadd232i_ASwizzle(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Hadd232i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hadd232i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hadd232i_NegA(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Hadd232i_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hadd232i_Ftz(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Hadd2C_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hadd2C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hadd2C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Hadd2C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Hadd2C_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hadd2C_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hadd2C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hadd2C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hadd2C_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hadd2C_NegB(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Hadd2C_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hadd2C_AbsB(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Hadd2C_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hadd2C_Ftz(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define Hadd2I_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hadd2I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hadd2I_BimmH0(o) ((int64_t)((((o) >> 20) & 0x3FF)))
#define Hadd2I_BimmH1(o) ((int64_t)((((o) >> 47) & 0x200) | (((o) >> 30) & 0x1FF)))
#define Hadd2I_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hadd2I_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hadd2I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hadd2I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hadd2I_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hadd2I_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hadd2I_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hadd2I_Ftz(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define Hadd2R_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hadd2R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hadd2R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Hadd2R_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hadd2R_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hadd2R_BSwizzle(o) ((int64_t)((((o) >> 28) & 0x3)))
#define Hadd2R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hadd2R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hadd2R_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hadd2R_NegB(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Hadd2R_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hadd2R_AbsB(o) ((int64_t)(((o) & 0x40000000) != 0))
#define Hadd2R_Sat(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Hadd2R_Ftz(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define Hfma232i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hfma232i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hfma232i_Imm(o) ((int64_t)(((o) >> 20)))
#define Hfma232i_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hfma232i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hfma232i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hfma232i_NegC(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Hfma232i_Fmz(o) ((int64_t)((((o) >> 57) & 0x3)))
#define Hfma2C_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hfma2C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hfma2C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Hfma2C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Hfma2C_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Hfma2C_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hfma2C_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hfma2C_CSwizzle(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Hfma2C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hfma2C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hfma2C_NegA(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Hfma2C_NegC(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Hfma2C_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hfma2C_Fmz(o) ((int64_t)((((o) >> 57) & 0x3)))
#define Hfma2I_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hfma2I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hfma2I_BimmH0(o) ((int64_t)((((o) >> 20) & 0x3FF)))
#define Hfma2I_BimmH1(o) ((int64_t)((((o) >> 47) & 0x200) | (((o) >> 30) & 0x1FF)))
#define Hfma2I_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Hfma2I_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hfma2I_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hfma2I_CSwizzle(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Hfma2I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hfma2I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hfma2I_NegC(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Hfma2I_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hfma2I_Fmz(o) ((int64_t)((((o) >> 57) & 0x3)))
#define Hfma2R_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hfma2R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hfma2R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Hfma2R_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Hfma2R_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hfma2R_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hfma2R_BSwizzle(o) ((int64_t)((((o) >> 28) & 0x3)))
#define Hfma2R_CSwizzle(o) ((int64_t)((((o) >> 35) & 0x3)))
#define Hfma2R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hfma2R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hfma2R_NegA(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Hfma2R_NegC(o) ((int64_t)(((o) & 0x40000000) != 0))
#define Hfma2R_Sat(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Hfma2R_Fmz(o) ((int64_t)((((o) >> 37) & 0x3)))
#define Hfma2Rc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hfma2Rc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hfma2Rc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Hfma2Rc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Hfma2Rc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Hfma2Rc_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hfma2Rc_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hfma2Rc_CSwizzle(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Hfma2Rc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hfma2Rc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hfma2Rc_NegA(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Hfma2Rc_NegC(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Hfma2Rc_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hfma2Rc_Fmz(o) ((int64_t)((((o) >> 57) & 0x3)))
#define Hmul232i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hmul232i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hmul232i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Hmul232i_ASwizzle(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Hmul232i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hmul232i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hmul232i_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hmul232i_Fmz(o) ((int64_t)((((o) >> 55) & 0x3)))
#define Hmul2C_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hmul2C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hmul2C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Hmul2C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Hmul2C_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hmul2C_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hmul2C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hmul2C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hmul2C_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hmul2C_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hmul2C_AbsB(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Hmul2C_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hmul2C_Fmz(o) ((int64_t)((((o) >> 39) & 0x3)))
#define Hmul2I_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hmul2I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hmul2I_BimmH0(o) ((int64_t)((((o) >> 20) & 0x3FF)))
#define Hmul2I_BimmH1(o) ((int64_t)((((o) >> 47) & 0x200) | (((o) >> 30) & 0x1FF)))
#define Hmul2I_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hmul2I_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hmul2I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hmul2I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hmul2I_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hmul2I_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hmul2I_Sat(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Hmul2I_Fmz(o) ((int64_t)((((o) >> 39) & 0x3)))
#define Hmul2R_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hmul2R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hmul2R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Hmul2R_OFmt(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Hmul2R_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hmul2R_BSwizzle(o) ((int64_t)((((o) >> 28) & 0x3)))
#define Hmul2R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hmul2R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hmul2R_NegA(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Hmul2R_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hmul2R_AbsB(o) ((int64_t)(((o) & 0x40000000) != 0))
#define Hmul2R_Sat(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Hmul2R_Fmz(o) ((int64_t)((((o) >> 39) & 0x3)))
#define Hset2C_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hset2C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hset2C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Hset2C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Hset2C_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hset2C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hset2C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hset2C_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hset2C_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hset2C_NegB(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Hset2C_Bval(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define Hset2C_Cmp(o) ((int64_t)((((o) >> 49) & 0xF)))
#define Hset2C_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Hset2C_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Hset2C_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Hset2C_Ftz(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Hset2I_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hset2I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hset2I_BimmH0(o) ((int64_t)((((o) >> 20) & 0x3FF)))
#define Hset2I_BimmH1(o) ((int64_t)((((o) >> 47) & 0x200) | (((o) >> 30) & 0x1FF)))
#define Hset2I_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hset2I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hset2I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hset2I_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hset2I_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hset2I_Bval(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define Hset2I_Cmp(o) ((int64_t)((((o) >> 49) & 0xF)))
#define Hset2I_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Hset2I_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Hset2I_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Hset2I_Ftz(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Hset2R_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Hset2R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hset2R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Hset2R_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hset2R_BSwizzle(o) ((int64_t)((((o) >> 28) & 0x3)))
#define Hset2R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hset2R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hset2R_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hset2R_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hset2R_NegB(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Hset2R_AbsB(o) ((int64_t)(((o) & 0x40000000) != 0))
#define Hset2R_Bval(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Hset2R_Cmp(o) ((int64_t)((((o) >> 35) & 0xF)))
#define Hset2R_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Hset2R_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Hset2R_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Hset2R_Ftz(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Hsetp2C_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define Hsetp2C_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Hsetp2C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hsetp2C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Hsetp2C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Hsetp2C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hsetp2C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hsetp2C_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hsetp2C_NegB(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Hsetp2C_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hsetp2C_AbsB(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Hsetp2C_FComp(o) ((int64_t)((((o) >> 49) & 0xF)))
#define Hsetp2C_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Hsetp2C_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Hsetp2C_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Hsetp2C_Ftz(o) ((int64_t)(((o) & 0x40) != 0))
#define Hsetp2C_HAnd(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define Hsetp2C_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hsetp2I_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define Hsetp2I_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Hsetp2I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hsetp2I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hsetp2I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hsetp2I_BimmH0(o) ((int64_t)((((o) >> 20) & 0x3FF)))
#define Hsetp2I_BimmH1(o) ((int64_t)((((o) >> 47) & 0x200) | (((o) >> 30) & 0x1FF)))
#define Hsetp2I_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hsetp2I_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hsetp2I_FComp(o) ((int64_t)((((o) >> 49) & 0xF)))
#define Hsetp2I_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Hsetp2I_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Hsetp2I_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Hsetp2I_Ftz(o) ((int64_t)(((o) & 0x40) != 0))
#define Hsetp2I_HAnd(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define Hsetp2I_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hsetp2R_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define Hsetp2R_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Hsetp2R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Hsetp2R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Hsetp2R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Hsetp2R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Hsetp2R_NegA(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define Hsetp2R_NegB(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Hsetp2R_AbsA(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Hsetp2R_AbsB(o) ((int64_t)(((o) & 0x40000000) != 0))
#define Hsetp2R_FComp2(o) ((int64_t)((((o) >> 35) & 0xF)))
#define Hsetp2R_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Hsetp2R_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Hsetp2R_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Hsetp2R_Ftz(o) ((int64_t)(((o) & 0x40) != 0))
#define Hsetp2R_HAnd(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Hsetp2R_ASwizzle(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Hsetp2R_BSwizzle(o) ((int64_t)((((o) >> 28) & 0x3)))
#define I2fC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define I2fC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define I2fC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define I2fC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define I2fC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define I2fC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define I2fC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define I2fC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define I2fC_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define I2fC_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define I2fC_ISrcFmt(o) ((int64_t)(((((o) >> 11) & 0x4) | (((o) >> 10) & 0x3))))
#define I2fC_DstFmt(o) ((int64_t)((((o) >> 8) & 0x3)))
#define I2fI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define I2fI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define I2fI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define I2fI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define I2fI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define I2fI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define I2fI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define I2fI_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define I2fI_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define I2fI_ISrcFmt(o) ((int64_t)(((((o) >> 11) & 0x4) | (((o) >> 10) & 0x3))))
#define I2fI_DstFmt(o) ((int64_t)((((o) >> 8) & 0x3)))
#define I2fR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define I2fR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define I2fR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define I2fR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define I2fR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define I2fR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define I2fR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define I2fR_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define I2fR_RoundMode(o) ((int64_t)((((o) >> 39) & 0x3)))
#define I2fR_ISrcFmt(o) ((int64_t)(((((o) >> 11) & 0x4) | (((o) >> 10) & 0x3))))
#define I2fR_DstFmt(o) ((int64_t)((((o) >> 8) & 0x3)))
#define I2iC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define I2iC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define I2iC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define I2iC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define I2iC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define I2iC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define I2iC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define I2iC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define I2iC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define I2iC_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define I2iC_IDstFmt(o) ((int64_t)(((((o) >> 10) & 0x4) | (((o) >> 8) & 0x3))))
#define I2iC_ISrcFmt(o) ((int64_t)(((((o) >> 11) & 0x4) | (((o) >> 10) & 0x3))))
#define I2iI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define I2iI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define I2iI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define I2iI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define I2iI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define I2iI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define I2iI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define I2iI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define I2iI_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define I2iI_IDstFmt(o) ((int64_t)(((((o) >> 10) & 0x4) | (((o) >> 8) & 0x3))))
#define I2iI_ISrcFmt(o) ((int64_t)(((((o) >> 11) & 0x4) | (((o) >> 10) & 0x3))))
#define I2iR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define I2iR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define I2iR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define I2iR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define I2iR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define I2iR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define I2iR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define I2iR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define I2iR_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define I2iR_IDstFmt(o) ((int64_t)(((((o) >> 10) & 0x4) | (((o) >> 8) & 0x3))))
#define I2iR_ISrcFmt(o) ((int64_t)(((((o) >> 11) & 0x4) | (((o) >> 10) & 0x3))))
#define Iadd32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Iadd32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Iadd32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Iadd32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Iadd32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Iadd32i_AvgMode(o) ((int64_t)((((o) >> 55) & 0x3)))
#define Iadd32i_Sat(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Iadd32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Iadd32i_X(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define Iadd3C_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Iadd3C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Iadd3C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Iadd3C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Iadd3C_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Iadd3C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Iadd3C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Iadd3C_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Iadd3C_NegA(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Iadd3C_NegB(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Iadd3C_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Iadd3C_X(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Iadd3I_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Iadd3I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Iadd3I_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define Iadd3I_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Iadd3I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Iadd3I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Iadd3I_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Iadd3I_NegA(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Iadd3I_NegB(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Iadd3I_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Iadd3I_X(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Iadd3R_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Iadd3R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Iadd3R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Iadd3R_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Iadd3R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Iadd3R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Iadd3R_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Iadd3R_NegA(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Iadd3R_NegB(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Iadd3R_NegC(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Iadd3R_X(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Iadd3R_Lrs(o) ((int64_t)((((o) >> 37) & 0x3)))
#define Iadd3R_Apart(o) ((int64_t)((((o) >> 35) & 0x3)))
#define Iadd3R_Bpart(o) ((int64_t)((((o) >> 33) & 0x3)))
#define Iadd3R_Cpart(o) ((int64_t)((((o) >> 31) & 0x3)))
#define IaddC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IaddC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IaddC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IaddC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IaddC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IaddC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IaddC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IaddC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define IaddC_AvgMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define IaddC_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IaddI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IaddI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IaddI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define IaddI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IaddI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IaddI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IaddI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define IaddI_AvgMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define IaddI_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IaddR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IaddR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IaddR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define IaddR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IaddR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IaddR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IaddR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define IaddR_AvgMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define IaddR_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IcmpC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IcmpC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IcmpC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IcmpC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IcmpC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define IcmpC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IcmpC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IcmpC_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IcmpC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IcmpI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IcmpI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IcmpI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define IcmpI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define IcmpI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IcmpI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IcmpI_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IcmpI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IcmpR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IcmpR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IcmpR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define IcmpR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define IcmpR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IcmpR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IcmpR_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IcmpR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IcmpRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IcmpRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IcmpRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define IcmpRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IcmpRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IcmpRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IcmpRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IcmpRc_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IcmpRc_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Ide_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Ide_Di(o) ((int64_t)(((o) & 0x20) != 0))
#define IdpC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IdpC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IdpC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IdpC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IdpC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define IdpC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IdpC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IdpC_IsHi(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define IdpC_SrcASign(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define IdpC_IsDp(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IdpC_SrcBSign(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IdpR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IdpR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IdpR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define IdpR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define IdpR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IdpR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IdpR_IsHi(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define IdpR_SrcASign(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define IdpR_IsDp(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IdpR_SrcBSign(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Imad32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Imad32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Imad32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Imad32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Imad32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Imad32i_BSigned(o) ((int64_t)(((o) & 0x200000000000000) != 0))
#define Imad32i_AvgMode(o) ((int64_t)((((o) >> 55) & 0x3)))
#define Imad32i_ASigned(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Imad32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Imad32i_Hilo(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define ImadC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ImadC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ImadC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImadC_Hilo(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define ImadC_BSigned(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define ImadC_AvgMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define ImadC_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ImadC_X(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define ImadC_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImadI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define ImadI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImadI_Hilo(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define ImadI_BSigned(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define ImadI_AvgMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define ImadI_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ImadI_X(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define ImadI_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImadR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ImadR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImadR_Hilo(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define ImadR_BSigned(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define ImadR_AvgMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define ImadR_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ImadR_X(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define ImadR_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImadRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ImadRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ImadRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadRc_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImadRc_Hilo(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define ImadRc_BSigned(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define ImadRc_AvgMode(o) ((int64_t)((((o) >> 51) & 0x3)))
#define ImadRc_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ImadRc_X(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define ImadRc_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImadspC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadspC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadspC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ImadspC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ImadspC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadspC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadspC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadspC_ASelect(o) ((int64_t)((((o) >> 48) & 0x7)))
#define ImadspC_BSelect(o) ((int64_t)((((o) >> 53) & 0x3)))
#define ImadspC_CSelect(o) ((int64_t)(((((o) >> 50) & 0x6) | (((o) >> 48) & 0x1))))
#define ImadspI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadspI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadspI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define ImadspI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadspI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadspI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadspI_ASelect(o) ((int64_t)((((o) >> 48) & 0x7)))
#define ImadspI_BSelect(o) ((int64_t)((((o) >> 53) & 0x3)))
#define ImadspI_CSelect(o) ((int64_t)(((((o) >> 50) & 0x6) | (((o) >> 48) & 0x1))))
#define ImadspR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadspR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadspR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ImadspR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadspR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadspR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadspR_ASelect(o) ((int64_t)((((o) >> 48) & 0x7)))
#define ImadspR_BSelect(o) ((int64_t)((((o) >> 53) & 0x3)))
#define ImadspR_CSelect(o) ((int64_t)(((((o) >> 50) & 0x6) | (((o) >> 48) & 0x1))))
#define ImadspRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImadspRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImadspRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ImadspRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ImadspRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ImadspRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImadspRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImadspRc_ASelect(o) ((int64_t)((((o) >> 48) & 0x7)))
#define ImadspRc_BSelect(o) ((int64_t)((((o) >> 53) & 0x3)))
#define ImadspRc_CSelect(o) ((int64_t)(((((o) >> 50) & 0x6) | (((o) >> 48) & 0x1))))
#define ImnmxC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImnmxC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImnmxC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ImnmxC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ImnmxC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImnmxC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImnmxC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImnmxC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImnmxC_XMode(o) ((int64_t)((((o) >> 43) & 0x3)))
#define ImnmxC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define ImnmxC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define ImnmxI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImnmxI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImnmxI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define ImnmxI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImnmxI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImnmxI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImnmxI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImnmxI_XMode(o) ((int64_t)((((o) >> 43) & 0x3)))
#define ImnmxI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define ImnmxI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define ImnmxR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImnmxR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImnmxR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ImnmxR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImnmxR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImnmxR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImnmxR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ImnmxR_XMode(o) ((int64_t)((((o) >> 43) & 0x3)))
#define ImnmxR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define ImnmxR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Imul32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Imul32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Imul32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Imul32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Imul32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Imul32i_ASigned(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Imul32i_BSigned(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Imul32i_Hilo(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define Imul32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define ImulC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImulC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImulC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ImulC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ImulC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImulC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImulC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImulC_ASigned(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define ImulC_BSigned(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define ImulC_Hilo(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ImulI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImulI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImulI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define ImulI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImulI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImulI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImulI_ASigned(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define ImulI_BSigned(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define ImulI_Hilo(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ImulR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ImulR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ImulR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ImulR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ImulR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ImulR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ImulR_ASigned(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define ImulR_BSigned(o) ((int64_t)(((o) & 0x20000000000) != 0))
#define ImulR_Hilo(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define Ipa_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ipa_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ipa_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Ipa_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Ipa_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ipa_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ipa_IpaOp(o) ((int64_t)((((o) >> 54) & 0x3)))
#define Ipa_Msi(o) ((int64_t)((((o) >> 52) & 0x3)))
#define Ipa_Sat(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Ipa_Idx(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define Ipa_Imm10(o) ((int64_t)((((o) >> 28) & 0x3FF)))
#define Ipa_SrcPred(o) ((int64_t)((((o) >> 47) & 0x7)))
#define Ipa_SrcPredInv(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Isberd_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Isberd_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Isberd_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Isberd_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Isberd_AlSize(o) ((int64_t)((((o) >> 47) & 0x3)))
#define Isberd_IBase(o) ((int64_t)((((o) >> 33) & 0x3)))
#define Isberd_O(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Isberd_P(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Iscadd32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Iscadd32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Iscadd32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Iscadd32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Iscadd32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Iscadd32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Iscadd32i_Imm5(o) ((int64_t)((((o) >> 53) & 0x1F)))
#define IscaddC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IscaddC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IscaddC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IscaddC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IscaddC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IscaddC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IscaddC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IscaddC_Imm5(o) ((int64_t)((((o) >> 39) & 0x1F)))
#define IscaddC_AvgMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define IscaddI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IscaddI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IscaddI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define IscaddI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IscaddI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IscaddI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IscaddI_Imm5(o) ((int64_t)((((o) >> 39) & 0x1F)))
#define IscaddI_AvgMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define IscaddR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IscaddR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IscaddR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define IscaddR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IscaddR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IscaddR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IscaddR_Imm5(o) ((int64_t)((((o) >> 39) & 0x1F)))
#define IscaddR_AvgMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define IsetC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IsetC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IsetC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IsetC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IsetC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IsetC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IsetC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IsetC_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IsetC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IsetC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define IsetC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define IsetC_BVal(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define IsetC_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define IsetC_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IsetI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IsetI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IsetI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define IsetI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IsetI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IsetI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IsetI_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IsetI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IsetI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define IsetI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define IsetI_BVal(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define IsetI_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define IsetI_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IsetR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define IsetR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IsetR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define IsetR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IsetR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IsetR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define IsetR_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IsetR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IsetR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define IsetR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define IsetR_BVal(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define IsetR_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define IsetR_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IsetpC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IsetpC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define IsetpC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define IsetpC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IsetpC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IsetpC_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IsetpC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IsetpC_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define IsetpC_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IsetpC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define IsetpC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define IsetpC_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define IsetpC_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define IsetpI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IsetpI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define IsetpI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IsetpI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IsetpI_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IsetpI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IsetpI_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define IsetpI_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IsetpI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define IsetpI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define IsetpI_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define IsetpI_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define IsetpR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define IsetpR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define IsetpR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define IsetpR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define IsetpR_IComp(o) ((int64_t)((((o) >> 49) & 0x7)))
#define IsetpR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define IsetpR_Bop(o) ((int64_t)((((o) >> 45) & 0x3)))
#define IsetpR_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define IsetpR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define IsetpR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define IsetpR_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define IsetpR_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Jcal_Imm32(o) ((int64_t)(((o) >> 20)))
#define Jcal_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Jcal_Inc(o) ((int64_t)(((o) & 0x40) != 0))
#define Jmp_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Jmp_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Jmp_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Jmp_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Jmp_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Jmp_Imm32(o) ((int64_t)(((o) >> 20)))
#define Jmp_Lmt(o) ((int64_t)(((o) & 0x40) != 0))
#define Jmp_U(o) ((int64_t)(((o) & 0x80) != 0))
#define Jmx_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Jmx_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Jmx_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Jmx_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Jmx_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Jmx_Imm32(o) ((int64_t)(((o) >> 20)))
#define Jmx_Lmt(o) ((int64_t)(((o) & 0x40) != 0))
#define Kil_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Kil_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Kil_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Ld_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ld_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ld_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ld_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ld_SrcPred(o) ((int64_t)((((o) >> 58) & 0x7)))
#define Ld_CacheOp(o) ((int64_t)((((o) >> 56) & 0x3)))
#define Ld_LsSize(o) ((int64_t)((((o) >> 53) & 0x7)))
#define Ld_E(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Ld_Imm32(o) ((int64_t)(((o) >> 20)))
#define Ldc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ldc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ldc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ldc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ldc_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Ldc_AddressMode(o) ((int64_t)((((o) >> 44) & 0x3)))
#define Ldc_CbufSlot(o) ((int64_t)((((o) >> 36) & 0x1F)))
#define Ldc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Ldg_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ldg_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ldg_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ldg_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ldg_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Ldg_CacheOp(o) ((int64_t)((((o) >> 46) & 0x3)))
#define Ldg_E(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define Ldg_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Ldl_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Ldl_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Ldl_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ldl_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ldl_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Ldl_CacheOp(o) ((int64_t)((((o) >> 44) & 0x3)))
#define Ldl_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Lds_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Lds_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Lds_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Lds_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Lds_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Lds_U(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Lds_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define LeaC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LeaC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LeaC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define LeaC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define LeaC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LeaC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LeaC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LeaC_X(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define LeaC_NegA(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define LeaC_ImmU5(o) ((int64_t)((((o) >> 39) & 0x1F)))
#define LeaC_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LeaHiC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LeaHiC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LeaHiC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define LeaHiC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define LeaHiC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define LeaHiC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LeaHiC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LeaHiC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LeaHiC_X(o) ((int64_t)(((o) & 0x200000000000000) != 0))
#define LeaHiC_NegA(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define LeaHiC_ImmU5(o) ((int64_t)((((o) >> 51) & 0x1F)))
#define LeaHiC_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LeaHiR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LeaHiR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LeaHiR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define LeaHiR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define LeaHiR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LeaHiR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LeaHiR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LeaHiR_X(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define LeaHiR_NegA(o) ((int64_t)(((o) & 0x2000000000) != 0))
#define LeaHiR_ImmU5(o) ((int64_t)((((o) >> 28) & 0x1F)))
#define LeaHiR_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LeaI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LeaI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LeaI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define LeaI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LeaI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LeaI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LeaI_X(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define LeaI_NegA(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define LeaI_ImmU5(o) ((int64_t)((((o) >> 39) & 0x1F)))
#define LeaI_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LeaR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LeaR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LeaR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define LeaR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LeaR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LeaR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LeaR_X(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define LeaR_NegA(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define LeaR_ImmU5(o) ((int64_t)((((o) >> 39) & 0x1F)))
#define LeaR_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Longjmp_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Lop32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Lop32i_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Lop32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Lop32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Lop32i_WriteCC(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define Lop32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Lop32i_X(o) ((int64_t)(((o) & 0x200000000000000) != 0))
#define Lop32i_LogicOp(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Lop32i_NegA(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Lop32i_NegB(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Lop3C_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Lop3C_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Lop3C_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define Lop3C_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define Lop3C_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Lop3C_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Lop3C_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Lop3C_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Lop3C_X(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Lop3C_Imm(o) ((int64_t)((((o) >> 48) & 0xFF)))
#define Lop3I_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Lop3I_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Lop3I_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define Lop3I_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Lop3I_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Lop3I_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Lop3I_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Lop3I_X(o) ((int64_t)(((o) & 0x200000000000000) != 0))
#define Lop3I_Imm(o) ((int64_t)((((o) >> 48) & 0xFF)))
#define Lop3R_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Lop3R_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Lop3R_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Lop3R_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Lop3R_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Lop3R_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Lop3R_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Lop3R_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Lop3R_PredicateOp(o) ((int64_t)((((o) >> 36) & 0x3)))
#define Lop3R_X(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define Lop3R_Imm(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define LopC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LopC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LopC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define LopC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define LopC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LopC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LopC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LopC_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LopC_PredicateOp(o) ((int64_t)((((o) >> 44) & 0x3)))
#define LopC_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define LopC_LogicOp(o) ((int64_t)((((o) >> 41) & 0x3)))
#define LopC_NegA(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define LopC_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define LopI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LopI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LopI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define LopI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LopI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LopI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LopI_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LopI_PredicateOp(o) ((int64_t)((((o) >> 44) & 0x3)))
#define LopI_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define LopI_LogicOp(o) ((int64_t)((((o) >> 41) & 0x3)))
#define LopI_NegA(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define LopI_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define LopR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define LopR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define LopR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define LopR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define LopR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define LopR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define LopR_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define LopR_PredicateOp(o) ((int64_t)((((o) >> 44) & 0x3)))
#define LopR_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define LopR_Lop(o) ((int64_t)((((o) >> 41) & 0x3)))
#define LopR_NegA(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define LopR_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define Membar_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Membar_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Membar_Membar(o) ((int64_t)((((o) >> 8) & 0x3)))
#define Membar_Ivall(o) ((int64_t)(((o) & 0x3)))
#define Mov32i_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Mov32i_Imm32(o) ((int64_t)(((o) >> 20)))
#define Mov32i_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Mov32i_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Mov32i_QuadMask(o) ((int64_t)((((o) >> 12) & 0xF)))
#define MovC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define MovC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define MovC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define MovC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define MovC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define MovC_QuadMask(o) ((int64_t)((((o) >> 39) & 0xF)))
#define MovI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define MovI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define MovI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define MovI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define MovI_QuadMask(o) ((int64_t)((((o) >> 39) & 0xF)))
#define MovR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define MovR_SrcA(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define MovR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define MovR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define MovR_QuadMask(o) ((int64_t)((((o) >> 39) & 0xF)))
#define Mufu_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Mufu_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Mufu_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Mufu_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Mufu_MufuOp(o) ((int64_t)((((o) >> 20) & 0xF)))
#define Mufu_AbsA(o) ((int64_t)(((o) & 0x400000000000) != 0))
#define Mufu_NegA(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Mufu_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Nop_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Nop_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Nop_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Nop_Trig(o) ((int64_t)(((o) & 0x2000) != 0))
#define Nop_Ccc(o) ((int64_t)((((o) >> 8) & 0x1F)))
#define OutC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define OutC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define OutC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define OutC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define OutC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define OutC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define OutC_OutType(o) ((int64_t)((((o) >> 39) & 0x3)))
#define OutI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define OutI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define OutI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define OutI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define OutI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define OutI_OutType(o) ((int64_t)((((o) >> 39) & 0x3)))
#define OutR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define OutR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define OutR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define OutR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define OutR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define OutR_OutType(o) ((int64_t)((((o) >> 39) & 0x3)))
#define P2rC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define P2rC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define P2rC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define P2rC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define P2rC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define P2rC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define P2rC_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define P2rC_Ccpr(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define P2rI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define P2rI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define P2rI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define P2rI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define P2rI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define P2rI_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define P2rI_Ccpr(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define P2rR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define P2rR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define P2rR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define P2rR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define P2rR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define P2rR_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define P2rR_Ccpr(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define Pbk_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Pbk_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Pcnt_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Pcnt_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Pexit_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Pixld_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Pixld_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Pixld_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Pixld_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Pixld_DestPred(o) ((int64_t)((((o) >> 45) & 0x7)))
#define Pixld_PixMode(o) ((int64_t)((((o) >> 31) & 0x7)))
#define Pixld_Imm8(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Plongjmp_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Plongjmp_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define PopcC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PopcC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define PopcC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define PopcC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PopcC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PopcC_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define PopcI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PopcI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define PopcI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PopcI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PopcI_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define PopcR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PopcR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define PopcR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PopcR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PopcR_NegB(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define Pret_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define Pret_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Pret_Inc(o) ((int64_t)(((o) & 0x40) != 0))
#define PrmtC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PrmtC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define PrmtC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define PrmtC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define PrmtC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define PrmtC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PrmtC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PrmtC_PMode(o) ((int64_t)((((o) >> 48) & 0xF)))
#define PrmtI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PrmtI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define PrmtI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define PrmtI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define PrmtI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PrmtI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PrmtI_PMode(o) ((int64_t)((((o) >> 48) & 0xF)))
#define PrmtR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PrmtR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define PrmtR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define PrmtR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define PrmtR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PrmtR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PrmtR_PMode(o) ((int64_t)((((o) >> 48) & 0xF)))
#define PrmtRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define PrmtRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define PrmtRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define PrmtRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define PrmtRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define PrmtRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define PrmtRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define PrmtRc_PMode(o) ((int64_t)((((o) >> 48) & 0xF)))
#define Pset_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Pset_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Pset_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Pset_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Pset_Src2Pred(o) ((int64_t)((((o) >> 12) & 0x7)))
#define Pset_Src2PredInv(o) ((int64_t)(((o) & 0x8000) != 0))
#define Pset_Src1Pred(o) ((int64_t)((((o) >> 29) & 0x7)))
#define Pset_Src1PredInv(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Pset_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Pset_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Pset_BoolOpAB(o) ((int64_t)((((o) >> 24) & 0x3)))
#define Pset_BoolOpC(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Pset_BVal(o) ((int64_t)(((o) & 0x100000000000) != 0))
#define Psetp_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define Psetp_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Psetp_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Psetp_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Psetp_Src2Pred(o) ((int64_t)((((o) >> 12) & 0x7)))
#define Psetp_Src2PredInv(o) ((int64_t)(((o) & 0x8000) != 0))
#define Psetp_Src1Pred(o) ((int64_t)((((o) >> 29) & 0x7)))
#define Psetp_Src1PredInv(o) ((int64_t)(((o) & 0x100000000) != 0))
#define Psetp_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Psetp_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Psetp_BoolOpAB(o) ((int64_t)((((o) >> 24) & 0x3)))
#define Psetp_BoolOpC(o) ((int64_t)((((o) >> 45) & 0x3)))
#define R2b_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define R2b_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define R2b_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define R2b_Mode(o) ((int64_t)((((o) >> 32) & 0x3)))
#define R2b_Name(o) ((int64_t)((((o) >> 28) & 0xF)))
#define R2pC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define R2pC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define R2pC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define R2pC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define R2pC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define R2pC_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define R2pC_Ccpr(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define R2pI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define R2pI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define R2pI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define R2pI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define R2pI_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define R2pI_Ccpr(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define R2pR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define R2pR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define R2pR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define R2pR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define R2pR_ByteSel(o) ((int64_t)((((o) >> 41) & 0x3)))
#define R2pR_Ccpr(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define Red_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Red_SrcB(o) ((int64_t)(((o) & 0xFF)))
#define Red_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Red_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Red_Imm20(o) ((int64_t)((((o) >> 28) & 0xFFFFF)))
#define Red_RedSize(o) ((int64_t)((((o) >> 20) & 0x7)))
#define Red_RedOp(o) ((int64_t)((((o) >> 23) & 0x7)))
#define Red_E(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Ret_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Ret_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Ret_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define RroC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define RroC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define RroC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define RroC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define RroC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define RroC_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define RroC_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define RroC_RroOp(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define RroI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define RroI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define RroI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define RroI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define RroI_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define RroI_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define RroI_RroOp(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define RroR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define RroR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define RroR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define RroR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define RroR_AbsB(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define RroR_NegB(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define RroR_RroOp(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define S2r_Dest(o) ((int64_t)(((o) & 0xFF)))
#define S2r_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define S2r_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define S2r_SReg(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define SelC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SelC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SelC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define SelC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define SelC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SelC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SelC_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define SelC_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define SelI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SelI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SelI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define SelI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SelI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SelI_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define SelI_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define SelR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SelR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SelR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define SelR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SelR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SelR_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define SelR_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Setcrsptr_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Setlmembase_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShfLI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShfLI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShfLI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ShfLI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShfLI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShfLI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShfLI_M(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ShfLI_XModeShf(o) ((int64_t)((((o) >> 48) & 0x3)))
#define ShfLI_MaxShift(o) ((int64_t)((((o) >> 37) & 0x3)))
#define ShfLI_Imm6(o) ((int64_t)((((o) >> 20) & 0x3F)))
#define ShfLR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShfLR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShfLR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ShfLR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ShfLR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShfLR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShfLR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShfLR_M(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ShfLR_XModeShf(o) ((int64_t)((((o) >> 48) & 0x3)))
#define ShfLR_MaxShift(o) ((int64_t)((((o) >> 37) & 0x3)))
#define ShfRI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShfRI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShfRI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ShfRI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShfRI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShfRI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShfRI_M(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ShfRI_XModeShf(o) ((int64_t)((((o) >> 48) & 0x3)))
#define ShfRI_MaxShift(o) ((int64_t)((((o) >> 37) & 0x3)))
#define ShfRI_Imm6(o) ((int64_t)((((o) >> 20) & 0x3F)))
#define ShfRR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShfRR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShfRR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ShfRR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define ShfRR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShfRR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShfRR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShfRR_M(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define ShfRR_XModeShf(o) ((int64_t)((((o) >> 48) & 0x3)))
#define ShfRR_MaxShift(o) ((int64_t)((((o) >> 37) & 0x3)))
#define Shfl_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Shfl_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Shfl_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Shfl_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Shfl_SrcBImm(o) ((int64_t)((((o) >> 20) & 0x1F)))
#define Shfl_SrcCImm(o) ((int64_t)((((o) >> 34) & 0x1FFF)))
#define Shfl_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Shfl_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Shfl_ShflMode(o) ((int64_t)((((o) >> 30) & 0x3)))
#define Shfl_CFixShfl(o) ((int64_t)(((o) & 0x20000000) != 0))
#define Shfl_BFixShfl(o) ((int64_t)(((o) & 0x10000000) != 0))
#define Shfl_DestPred(o) ((int64_t)((((o) >> 48) & 0x7)))
#define ShlC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShlC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShlC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ShlC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ShlC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShlC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShlC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShlC_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define ShlC_M(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ShlI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShlI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShlI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define ShlI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShlI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShlI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShlI_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define ShlI_M(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ShlR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShlR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShlR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ShlR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShlR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShlR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShlR_X(o) ((int64_t)(((o) & 0x80000000000) != 0))
#define ShlR_M(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ShrC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShrC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShrC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define ShrC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define ShrC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShrC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShrC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShrC_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ShrC_XMode(o) ((int64_t)((((o) >> 43) & 0x3)))
#define ShrC_Brev(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define ShrC_M(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ShrI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShrI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShrI_Imm20(o) ((int64_t)((((o) >> 37) & 0x80000) | (((o) >> 20) & 0x7FFFF)))
#define ShrI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShrI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShrI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShrI_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ShrI_XMode(o) ((int64_t)((((o) >> 43) & 0x3)))
#define ShrI_Brev(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define ShrI_M(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define ShrR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define ShrR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define ShrR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define ShrR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define ShrR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define ShrR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define ShrR_Signed(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define ShrR_XMode(o) ((int64_t)((((o) >> 43) & 0x3)))
#define ShrR_Brev(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define ShrR_M(o) ((int64_t)(((o) & 0x8000000000) != 0))
#define Ssy_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Ssy_Ca(o) ((int64_t)(((o) & 0x20) != 0))
#define St_Dest(o) ((int64_t)(((o) & 0xFF)))
#define St_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define St_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define St_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define St_SrcPred(o) ((int64_t)((((o) >> 58) & 0x7)))
#define St_CacheOp(o) ((int64_t)((((o) >> 56) & 0x3)))
#define St_LsSize(o) ((int64_t)((((o) >> 53) & 0x7)))
#define St_E(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define St_Imm32(o) ((int64_t)(((o) >> 20)))
#define Stg_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Stg_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Stg_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Stg_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Stg_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Stg_CacheOp(o) ((int64_t)((((o) >> 46) & 0x3)))
#define Stg_E(o) ((int64_t)(((o) & 0x200000000000) != 0))
#define Stg_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Stl_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Stl_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Stl_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Stl_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Stl_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Stl_CacheOp(o) ((int64_t)((((o) >> 44) & 0x3)))
#define Stl_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Stp_Wait(o) ((int64_t)(((o) & 0x80000000) != 0))
#define Stp_Imm8(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Sts_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Sts_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Sts_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Sts_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Sts_LsSize(o) ((int64_t)((((o) >> 48) & 0x7)))
#define Sts_Imm24(o) ((int64_t)((((o) >> 20) & 0xFFFFFF)))
#define Suatom_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Suatom_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Suatom_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Suatom_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Suatom_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Suatom_Size(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Suatom_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Suatom_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Suatom_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define Suatom_Op(o) ((int64_t)((((o) >> 29) & 0xF)))
#define Suatom_Ba(o) ((int64_t)(((o) & 0x10000000) != 0))
#define SuatomB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuatomB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuatomB_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define SuatomB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SuatomB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuatomB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuatomB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuatomB_Size(o) ((int64_t)((((o) >> 36) & 0x7)))
#define SuatomB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuatomB_Op(o) ((int64_t)((((o) >> 29) & 0xF)))
#define SuatomB_Ba(o) ((int64_t)(((o) & 0x10000000) != 0))
#define SuatomB2_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuatomB2_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuatomB2_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define SuatomB2_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SuatomB2_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuatomB2_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuatomB2_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define SuatomB2_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuatomB2_Size(o) ((int64_t)((((o) >> 36) & 0x7)))
#define SuatomB2_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuatomB2_Op(o) ((int64_t)((((o) >> 29) & 0xF)))
#define SuatomB2_Ba(o) ((int64_t)(((o) & 0x10000000) != 0))
#define SuatomCas_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuatomCas_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuatomCas_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define SuatomCas_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuatomCas_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuatomCas_Size(o) ((int64_t)((((o) >> 51) & 0x7)))
#define SuatomCas_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuatomCas_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define SuatomCas_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuatomCas_DestPred(o) ((int64_t)((((o) >> 30) & 0x7)))
#define SuatomCas_Ba(o) ((int64_t)(((o) & 0x10000000) != 0))
#define SuatomCasB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuatomCasB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuatomCasB_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define SuatomCasB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SuatomCasB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuatomCasB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuatomCasB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuatomCasB_Size(o) ((int64_t)((((o) >> 36) & 0x7)))
#define SuatomCasB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuatomCasB_DestPred(o) ((int64_t)((((o) >> 30) & 0x7)))
#define SuatomCasB_Ba(o) ((int64_t)(((o) & 0x10000000) != 0))
#define Suld_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Suld_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Suld_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Suld_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Suld_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Suld_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Suld_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define Suld_DestPred2(o) ((int64_t)((((o) >> 30) & 0x7)))
#define Suld_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define Suld_Rgba(o) ((int64_t)((((o) >> 20) & 0xF)))
#define SuldB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuldB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuldB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SuldB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuldB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuldB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuldB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuldB_DestPred2(o) ((int64_t)((((o) >> 30) & 0x7)))
#define SuldB_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define SuldB_Rgba(o) ((int64_t)((((o) >> 20) & 0xF)))
#define SuldD_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuldD_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuldD_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuldD_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuldD_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuldD_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define SuldD_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuldD_DestPred2(o) ((int64_t)((((o) >> 30) & 0x7)))
#define SuldD_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define SuldD_Ba(o) ((int64_t)(((o) & 0x800000) != 0))
#define SuldD_Size(o) ((int64_t)((((o) >> 20) & 0x7)))
#define SuldDB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuldDB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuldDB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SuldDB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuldDB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuldDB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuldDB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuldDB_DestPred2(o) ((int64_t)((((o) >> 30) & 0x7)))
#define SuldDB_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define SuldDB_Ba(o) ((int64_t)(((o) & 0x800000) != 0))
#define SuldDB_Size(o) ((int64_t)((((o) >> 20) & 0x7)))
#define Sured_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Sured_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Sured_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Sured_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Sured_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Sured_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Sured_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define Sured_Op(o) ((int64_t)((((o) >> 24) & 0x7)))
#define Sured_Ba(o) ((int64_t)(((o) & 0x800000) != 0))
#define Sured_Size(o) ((int64_t)((((o) >> 20) & 0x7)))
#define SuredB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SuredB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SuredB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SuredB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SuredB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SuredB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SuredB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SuredB_Op(o) ((int64_t)((((o) >> 24) & 0x7)))
#define SuredB_Ba(o) ((int64_t)(((o) & 0x800000) != 0))
#define SuredB_Size(o) ((int64_t)((((o) >> 20) & 0x7)))
#define Sust_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Sust_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Sust_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Sust_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Sust_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define Sust_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Sust_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define Sust_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define Sust_Rgba(o) ((int64_t)((((o) >> 20) & 0xF)))
#define SustB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SustB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SustB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SustB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SustB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SustB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SustB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SustB_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define SustB_Rgba(o) ((int64_t)((((o) >> 20) & 0xF)))
#define SustD_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SustD_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SustD_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SustD_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SustD_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SustD_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define SustD_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SustD_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define SustD_Ba(o) ((int64_t)(((o) & 0x800000) != 0))
#define SustD_Size(o) ((int64_t)((((o) >> 20) & 0x7)))
#define SustDB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define SustDB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define SustDB_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define SustDB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define SustDB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define SustDB_Clamp(o) ((int64_t)((((o) >> 49) & 0x3)))
#define SustDB_Dim(o) ((int64_t)((((o) >> 33) & 0x7)))
#define SustDB_CacheOp(o) ((int64_t)((((o) >> 24) & 0x3)))
#define SustDB_Ba(o) ((int64_t)(((o) & 0x800000) != 0))
#define SustDB_Size(o) ((int64_t)((((o) >> 20) & 0x7)))
#define Sync_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Sync_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Sync_Ccc(o) ((int64_t)(((o) & 0x1F)))
#define Tex_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tex_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tex_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tex_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tex_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tex_Lc(o) ((int64_t)(((o) & 0x400000000000000) != 0))
#define Tex_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Tex_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tex_Lod(o) ((int64_t)((((o) >> 55) & 0x7)))
#define Tex_Aoffi(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Tex_Dc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Tex_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Tex_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Tex_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Tex_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TexB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TexB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TexB_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define TexB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TexB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TexB_Lcb(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define TexB_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define TexB_Lodb(o) ((int64_t)((((o) >> 37) & 0x7)))
#define TexB_Aoffib(o) ((int64_t)(((o) & 0x1000000000) != 0))
#define TexB_Dc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define TexB_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define TexB_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define TexB_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define TexB_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Texs_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Texs_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Texs_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Texs_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Texs_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Texs_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Texs_Target(o) ((int64_t)((((o) >> 53) & 0xF)))
#define Texs_WMask(o) ((int64_t)((((o) >> 50) & 0x7)))
#define Texs_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Texs_Dest2(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define TexsF16_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TexsF16_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TexsF16_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define TexsF16_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TexsF16_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TexsF16_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define TexsF16_Target(o) ((int64_t)((((o) >> 53) & 0xF)))
#define TexsF16_WMask(o) ((int64_t)((((o) >> 50) & 0x7)))
#define TexsF16_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TexsF16_Dest2(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define Tld_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tld_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tld_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tld_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tld_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tld_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tld_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Tld_Lod(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Tld_Toff(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Tld_Ms(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Tld_Cl(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Tld_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tld_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Tld_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Tld4_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tld4_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tld4_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tld4_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tld4_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tld4_Lc(o) ((int64_t)(((o) & 0x400000000000000) != 0))
#define Tld4_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Tld4_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tld4_TexComp(o) ((int64_t)((((o) >> 56) & 0x3)))
#define Tld4_Toff(o) ((int64_t)((((o) >> 54) & 0x3)))
#define Tld4_Dc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Tld4_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Tld4_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Tld4_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Tld4_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tld4B_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tld4B_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tld4B_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tld4B_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tld4B_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tld4B_Lc(o) ((int64_t)(((o) & 0x10000000000) != 0))
#define Tld4B_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Tld4B_TexComp(o) ((int64_t)((((o) >> 38) & 0x3)))
#define Tld4B_Toff(o) ((int64_t)((((o) >> 36) & 0x3)))
#define Tld4B_Dc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Tld4B_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Tld4B_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Tld4B_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Tld4B_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tld4s_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tld4s_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tld4s_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tld4s_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tld4s_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tld4s_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tld4s_TexComp(o) ((int64_t)((((o) >> 52) & 0x3)))
#define Tld4s_Aoffi(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Tld4s_Dc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Tld4s_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tld4s_Dest2(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define Tld4sF16_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tld4sF16_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tld4sF16_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tld4sF16_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tld4sF16_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tld4sF16_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tld4sF16_TexComp(o) ((int64_t)((((o) >> 52) & 0x3)))
#define Tld4sF16_Aoffi(o) ((int64_t)(((o) & 0x8000000000000) != 0))
#define Tld4sF16_Dc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Tld4sF16_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tld4sF16_Dest2(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define TldB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TldB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TldB_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define TldB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TldB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TldB_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define TldB_Lod(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define TldB_Toff(o) ((int64_t)(((o) & 0x800000000) != 0))
#define TldB_Ms(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define TldB_Cl(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define TldB_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TldB_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define TldB_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Tlds_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tlds_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tlds_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tlds_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tlds_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tlds_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tlds_Target(o) ((int64_t)((((o) >> 53) & 0xF)))
#define Tlds_WMask(o) ((int64_t)((((o) >> 50) & 0x7)))
#define Tlds_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tlds_Dest2(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define TldsF16_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TldsF16_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TldsF16_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define TldsF16_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TldsF16_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TldsF16_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define TldsF16_Target(o) ((int64_t)((((o) >> 53) & 0xF)))
#define TldsF16_WMask(o) ((int64_t)((((o) >> 50) & 0x7)))
#define TldsF16_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TldsF16_Dest2(o) ((int64_t)((((o) >> 28) & 0xFF)))
#define Tmml_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Tmml_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Tmml_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Tmml_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Tmml_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Tmml_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Tmml_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Tmml_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Tmml_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Tmml_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define TmmlB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TmmlB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TmmlB_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define TmmlB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TmmlB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TmmlB_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TmmlB_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define TmmlB_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define TmmlB_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Txa_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Txa_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Txa_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Txa_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Txa_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Txa_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Txa_Ndv(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Txa_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Txd_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Txd_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Txd_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Txd_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Txd_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Txd_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Txd_Lc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Txd_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Txd_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Txd_Toff(o) ((int64_t)(((o) & 0x800000000) != 0))
#define Txd_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Txd_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define TxdB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TxdB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TxdB_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define TxdB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TxdB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TxdB_DestPred(o) ((int64_t)((((o) >> 51) & 0x7)))
#define TxdB_Lc(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define TxdB_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TxdB_Toff(o) ((int64_t)(((o) & 0x800000000) != 0))
#define TxdB_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define TxdB_Dim(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Txq_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Txq_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Txq_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Txq_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Txq_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Txq_TidB(o) ((int64_t)((((o) >> 36) & 0x1FFF)))
#define Txq_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define Txq_TexQuery(o) ((int64_t)((((o) >> 22) & 0x3F)))
#define TxqB_Dest(o) ((int64_t)(((o) & 0xFF)))
#define TxqB_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define TxqB_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define TxqB_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define TxqB_Nodep(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define TxqB_WMask(o) ((int64_t)((((o) >> 31) & 0xF)))
#define TxqB_TexQuery(o) ((int64_t)((((o) >> 22) & 0x3F)))
#define Vabsdiff_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vabsdiff_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vabsdiff_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vabsdiff_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vabsdiff_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vabsdiff_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vabsdiff_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vabsdiff_DFormat(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Vabsdiff_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vabsdiff_BSelect(o) ((int64_t)(((((o) >> 46) & 0x8) | (((o) >> 28) & 0x7))))
#define Vabsdiff_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Vabsdiff_VideoOp(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Vabsdiff_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vabsdiff4_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vabsdiff4_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vabsdiff4_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vabsdiff4_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vabsdiff4_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vabsdiff4_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vabsdiff4_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vabsdiff4_VRed(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Vabsdiff4_LaneMask4(o) ((int64_t)(((((o) >> 49) & 0xC) | (((o) >> 36) & 0x3))))
#define Vabsdiff4_Sat(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vabsdiff4_SrcBFmt(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Vabsdiff4_SrcAFmt(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define Vabsdiff4_DFormat(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define Vabsdiff4_Asel4(o) ((int64_t)((((o) >> 32) & 0xF)))
#define Vabsdiff4_Bsel4(o) ((int64_t)((((o) >> 28) & 0xF)))
#define Vadd_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vadd_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vadd_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vadd_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vadd_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vadd_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vadd_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Vadd_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vadd_AvgMode(o) ((int64_t)((((o) >> 56) & 0x3)))
#define Vadd_DFormat(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Vadd_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vadd_BSelect(o) ((int64_t)(((((o) >> 46) & 0x8) | (((o) >> 28) & 0x7))))
#define Vadd_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Vadd_VideoOp(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Vadd_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vmad_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vmad_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vmad_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vmad_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vmad_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vmad_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vmad_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Vmad_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vmad_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vmad_BSelect(o) ((int64_t)(((((o) >> 46) & 0x8) | (((o) >> 28) & 0x7))))
#define Vmad_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Vmad_AvgMode(o) ((int64_t)((((o) >> 53) & 0x3)))
#define Vmad_VideoScale(o) ((int64_t)((((o) >> 51) & 0x3)))
#define Vmad_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vmnmx_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vmnmx_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vmnmx_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vmnmx_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vmnmx_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vmnmx_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vmnmx_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Vmnmx_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vmnmx_DFormat(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Vmnmx_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vmnmx_BSelect(o) ((int64_t)(((((o) >> 46) & 0x8) | (((o) >> 28) & 0x7))))
#define Vmnmx_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Vmnmx_VideoOp(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Vmnmx_Mn(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define Vmnmx_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vote_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vote_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vote_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vote_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Vote_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Vote_VoteMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define Vote_VpDest(o) ((int64_t)((((o) >> 45) & 0x7)))
#define Votevtg_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Votevtg_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Votevtg_VoteMode(o) ((int64_t)((((o) >> 48) & 0x3)))
#define Votevtg_Imm28(o) ((int64_t)((((o) >> 20) & 0xFFFFFFF)))
#define Vset_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vset_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vset_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vset_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vset_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vset_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vset_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vset_VComp(o) ((int64_t)((((o) >> 54) & 0x7)))
#define Vset_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vset_BSelect(o) ((int64_t)(((((o) >> 46) & 0x8) | (((o) >> 28) & 0x7))))
#define Vset_VideoOp(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Vset_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vsetp_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vsetp_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vsetp_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vsetp_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vsetp_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define Vsetp_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vsetp_BSelect(o) ((int64_t)(((((o) >> 46) & 0x8) | (((o) >> 28) & 0x7))))
#define Vsetp_VComp(o) ((int64_t)(((((o) >> 45) & 0x4) | (((o) >> 43) & 0x3))))
#define Vsetp_BoolOp(o) ((int64_t)((((o) >> 45) & 0x3)))
#define Vsetp_SrcPred(o) ((int64_t)((((o) >> 39) & 0x7)))
#define Vsetp_SrcPredInv(o) ((int64_t)(((o) & 0x40000000000) != 0))
#define Vsetp_DestPred(o) ((int64_t)((((o) >> 3) & 0x7)))
#define Vsetp_DestPredInv(o) ((int64_t)(((o) & 0x7)))
#define Vsetp_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vshl_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vshl_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vshl_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vshl_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vshl_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vshl_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vshl_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vshl_Mv(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Vshl_DFormat(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Vshl_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vshl_BSelect(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Vshl_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Vshl_VideoOp(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Vshl_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define Vshr_Dest(o) ((int64_t)(((o) & 0xFF)))
#define Vshr_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define Vshr_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define Vshr_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define Vshr_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define Vshr_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define Vshr_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define Vshr_Mv(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define Vshr_DFormat(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define Vshr_ASelect(o) ((int64_t)(((((o) >> 45) & 0x8) | (((o) >> 36) & 0x7))))
#define Vshr_BSelect(o) ((int64_t)((((o) >> 28) & 0x7)))
#define Vshr_Sat(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define Vshr_VideoOp(o) ((int64_t)((((o) >> 51) & 0x7)))
#define Vshr_BVideo(o) ((int64_t)(((o) & 0x4000000000000) != 0))
#define XmadC_Dest(o) ((int64_t)(((o) & 0xFF)))
#define XmadC_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define XmadC_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define XmadC_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define XmadC_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define XmadC_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define XmadC_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define XmadC_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define XmadC_Mrg(o) ((int64_t)(((o) & 0x100000000000000) != 0))
#define XmadC_Psl(o) ((int64_t)(((o) & 0x80000000000000) != 0))
#define XmadC_X(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define XmadC_HiloA(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define XmadC_HiloB(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define XmadC_XmadCop(o) ((int64_t)((((o) >> 50) & 0x3)))
#define XmadC_BSigned(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define XmadC_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define XmadI_Dest(o) ((int64_t)(((o) & 0xFF)))
#define XmadI_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define XmadI_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define XmadI_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define XmadI_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define XmadI_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define XmadI_Imm16(o) ((int64_t)((((o) >> 20) & 0xFFFF)))
#define XmadI_HiloA(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define XmadI_XmadCop(o) ((int64_t)((((o) >> 50) & 0x7)))
#define XmadI_BSigned(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define XmadI_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define XmadI_X(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define XmadI_Mrg(o) ((int64_t)(((o) & 0x2000000000) != 0))
#define XmadI_Psl(o) ((int64_t)(((o) & 0x1000000000) != 0))
#define XmadR_Dest(o) ((int64_t)(((o) & 0xFF)))
#define XmadR_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define XmadR_SrcB(o) ((int64_t)((((o) >> 20) & 0xFF)))
#define XmadR_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define XmadR_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define XmadR_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define XmadR_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define XmadR_HiloA(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define XmadR_XmadCop(o) ((int64_t)((((o) >> 50) & 0x7)))
#define XmadR_BSigned(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define XmadR_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))
#define XmadR_X(o) ((int64_t)(((o) & 0x4000000000) != 0))
#define XmadR_Mrg(o) ((int64_t)(((o) & 0x2000000000) != 0))
#define XmadR_Psl(o) ((int64_t)(((o) & 0x1000000000) != 0))
#define XmadR_HiloB(o) ((int64_t)(((o) & 0x800000000) != 0))
#define XmadRc_Dest(o) ((int64_t)(((o) & 0xFF)))
#define XmadRc_SrcA(o) ((int64_t)((((o) >> 8) & 0xFF)))
#define XmadRc_SrcC(o) ((int64_t)((((o) >> 39) & 0xFF)))
#define XmadRc_CbufSlot(o) ((int64_t)((((o) >> 34) & 0x1F)))
#define XmadRc_CbufOffset(o) ((int64_t)((((o) >> 20) & 0x3FFF)))
#define XmadRc_Pred(o) ((int64_t)((((o) >> 16) & 0x7)))
#define XmadRc_PredInv(o) ((int64_t)(((o) & 0x80000) != 0))
#define XmadRc_WriteCC(o) ((int64_t)(((o) & 0x800000000000) != 0))
#define XmadRc_X(o) ((int64_t)(((o) & 0x40000000000000) != 0))
#define XmadRc_HiloA(o) ((int64_t)(((o) & 0x20000000000000) != 0))
#define XmadRc_HiloB(o) ((int64_t)(((o) & 0x10000000000000) != 0))
#define XmadRc_XmadCop(o) ((int64_t)((((o) >> 50) & 0x3)))
#define XmadRc_BSigned(o) ((int64_t)(((o) & 0x2000000000000) != 0))
#define XmadRc_ASigned(o) ((int64_t)(((o) & 0x1000000000000) != 0))

enum MxField {
    MXF_AFixBar,
    MXF_ASelect,
    MXF_ASigned,
    MXF_ASwizzle,
    MXF_AbsA,
    MXF_AbsB,
    MXF_AddressMode,
    MXF_Aio,
    MXF_AlSize,
    MXF_Aoffi,
    MXF_Aoffib,
    MXF_Apart,
    MXF_Asel4,
    MXF_AtomOp,
    MXF_AtomsBcRz,
    MXF_AtomsSize,
    MXF_AvgMode,
    MXF_BFixBar,
    MXF_BFixShfl,
    MXF_BSelect,
    MXF_BSigned,
    MXF_BSwizzle,
    MXF_BVal,
    MXF_BVideo,
    MXF_Ba,
    MXF_BarOp,
    MXF_BarRedOp,
    MXF_BcRz,
    MXF_BimmH0,
    MXF_BimmH1,
    MXF_BoolOp,
    MXF_BoolOpAB,
    MXF_BoolOpC,
    MXF_Bop,
    MXF_Bpart,
    MXF_Bpt,
    MXF_Brev,
    MXF_Bsel4,
    MXF_Bval,
    MXF_ByteSel,
    MXF_CFixShfl,
    MXF_CSelect,
    MXF_CSwizzle,
    MXF_Ca,
    MXF_Cache,
    MXF_CacheOp,
    MXF_CbufOffset,
    MXF_CbufSlot,
    MXF_Ccc,
    MXF_Ccpr,
    MXF_CctlOp,
    MXF_CctltOp,
    MXF_ChkModeF,
    MXF_Cl,
    MXF_Clamp,
    MXF_Cmp,
    MXF_Cpart,
    MXF_DFormat,
    MXF_Dc,
    MXF_Dest,
    MXF_Dest2,
    MXF_DestPred,
    MXF_DestPred2,
    MXF_DestPredInv,
    MXF_Di,
    MXF_Dim,
    MXF_DstFmt,
    MXF_E,
    MXF_FComp,
    MXF_FComp2,
    MXF_Fmz,
    MXF_Ftz,
    MXF_HAnd,
    MXF_Hilo,
    MXF_HiloA,
    MXF_HiloB,
    MXF_IBase,
    MXF_IComp,
    MXF_IDstFmt,
    MXF_ISrcFmt,
    MXF_Idx,
    MXF_Imm,
    MXF_Imm10,
    MXF_Imm11,
    MXF_Imm12,
    MXF_Imm16,
    MXF_Imm20,
    MXF_Imm22,
    MXF_Imm24,
    MXF_Imm28,
    MXF_Imm30,
    MXF_Imm32,
    MXF_Imm5,
    MXF_Imm6,
    MXF_Imm8,
    MXF_ImmU5,
    MXF_Inc,
    MXF_IpaOp,
    MXF_IsDp,
    MXF_IsHi,
    MXF_Ivall,
    MXF_KeepRefCnt,
    MXF_LaneMask4,
    MXF_Lc,
    MXF_Lcb,
    MXF_Le,
    MXF_Lmt,
    MXF_Lod,
    MXF_Lodb,
    MXF_LogicOp,
    MXF_Lop,
    MXF_Lrs,
    MXF_LsSize,
    MXF_M,
    MXF_MaxShift,
    MXF_Membar,
    MXF_Mn,
    MXF_Mode,
    MXF_Mrg,
    MXF_Ms,
    MXF_Msi,
    MXF_MufuOp,
    MXF_Mv,
    MXF_Name,
    MXF_Ndv,
    MXF_NegA,
    MXF_NegB,
    MXF_NegC,
    MXF_Nodep,
    MXF_O,
    MXF_OFmt,
    MXF_Op,
    MXF_OutType,
    MXF_P,
    MXF_PMode,
    MXF_PendCnt,
    MXF_PixMode,
    MXF_PnWord,
    MXF_Pred,
    MXF_PredInv,
    MXF_PredicateOp,
    MXF_Psl,
    MXF_QuadMask,
    MXF_RedOp,
    MXF_RedSize,
    MXF_Rgba,
    MXF_RoundMode,
    MXF_RroOp,
    MXF_SReg,
    MXF_Sat,
    MXF_Sbid,
    MXF_Scale,
    MXF_Sh,
    MXF_ShflMode,
    MXF_Signed,
    MXF_Size,
    MXF_Src1Pred,
    MXF_Src1PredInv,
    MXF_Src2Pred,
    MXF_Src2PredInv,
    MXF_SrcA,
    MXF_SrcAFmt,
    MXF_SrcASign,
    MXF_SrcB,
    MXF_SrcBFmt,
    MXF_SrcBImm,
    MXF_SrcBSign,
    MXF_SrcC,
    MXF_SrcCImm,
    MXF_SrcFmt,
    MXF_SrcPred,
    MXF_SrcPredInv,
    MXF_Target,
    MXF_TexComp,
    MXF_TexQuery,
    MXF_TidB,
    MXF_Toff,
    MXF_Trig,
    MXF_TsIdx13,
    MXF_U,
    MXF_VComp,
    MXF_VRed,
    MXF_VideoOp,
    MXF_VideoScale,
    MXF_VoteMode,
    MXF_VpDest,
    MXF_WMask,
    MXF_Wait,
    MXF_WriteCC,
    MXF_X,
    MXF_XMode,
    MXF_XModeShf,
    MXF_XmadCop,
    MXF_COUNT
};
#define MX_NO INT64_MIN
static inline int64_t mx_get(int op, int f, uint64_t o) {
    switch (op) {
    case MXOP_Al2p:
        switch (f) {
        case MXF_Dest: return Al2p_Dest(o);
        case MXF_SrcA: return Al2p_SrcA(o);
        case MXF_Pred: return Al2p_Pred(o);
        case MXF_PredInv: return Al2p_PredInv(o);
        case MXF_AlSize: return Al2p_AlSize(o);
        case MXF_Aio: return Al2p_Aio(o);
        case MXF_Imm11: return Al2p_Imm11(o);
        case MXF_DestPred: return Al2p_DestPred(o);
        }
        return MX_NO;
    case MXOP_Ald:
        switch (f) {
        case MXF_Dest: return Ald_Dest(o);
        case MXF_SrcA: return Ald_SrcA(o);
        case MXF_SrcB: return Ald_SrcB(o);
        case MXF_Pred: return Ald_Pred(o);
        case MXF_PredInv: return Ald_PredInv(o);
        case MXF_Imm11: return Ald_Imm11(o);
        case MXF_P: return Ald_P(o);
        case MXF_O: return Ald_O(o);
        case MXF_AlSize: return Ald_AlSize(o);
        }
        return MX_NO;
    case MXOP_Ast:
        switch (f) {
        case MXF_SrcA: return Ast_SrcA(o);
        case MXF_SrcB: return Ast_SrcB(o);
        case MXF_SrcC: return Ast_SrcC(o);
        case MXF_Pred: return Ast_Pred(o);
        case MXF_PredInv: return Ast_PredInv(o);
        case MXF_Imm11: return Ast_Imm11(o);
        case MXF_P: return Ast_P(o);
        case MXF_AlSize: return Ast_AlSize(o);
        }
        return MX_NO;
    case MXOP_Atom:
        switch (f) {
        case MXF_Dest: return Atom_Dest(o);
        case MXF_SrcA: return Atom_SrcA(o);
        case MXF_SrcB: return Atom_SrcB(o);
        case MXF_Pred: return Atom_Pred(o);
        case MXF_PredInv: return Atom_PredInv(o);
        case MXF_Imm20: return Atom_Imm20(o);
        case MXF_Size: return Atom_Size(o);
        case MXF_Op: return Atom_Op(o);
        case MXF_E: return Atom_E(o);
        }
        return MX_NO;
    case MXOP_AtomCas:
        switch (f) {
        case MXF_Dest: return AtomCas_Dest(o);
        case MXF_SrcA: return AtomCas_SrcA(o);
        case MXF_SrcB: return AtomCas_SrcB(o);
        case MXF_Pred: return AtomCas_Pred(o);
        case MXF_PredInv: return AtomCas_PredInv(o);
        case MXF_BcRz: return AtomCas_BcRz(o);
        case MXF_E: return AtomCas_E(o);
        }
        return MX_NO;
    case MXOP_Atoms:
        switch (f) {
        case MXF_Dest: return Atoms_Dest(o);
        case MXF_SrcA: return Atoms_SrcA(o);
        case MXF_SrcB: return Atoms_SrcB(o);
        case MXF_Pred: return Atoms_Pred(o);
        case MXF_PredInv: return Atoms_PredInv(o);
        case MXF_Imm22: return Atoms_Imm22(o);
        case MXF_AtomsSize: return Atoms_AtomsSize(o);
        case MXF_AtomOp: return Atoms_AtomOp(o);
        }
        return MX_NO;
    case MXOP_AtomsCas:
        switch (f) {
        case MXF_Dest: return AtomsCas_Dest(o);
        case MXF_SrcA: return AtomsCas_SrcA(o);
        case MXF_SrcB: return AtomsCas_SrcB(o);
        case MXF_Pred: return AtomsCas_Pred(o);
        case MXF_PredInv: return AtomsCas_PredInv(o);
        case MXF_AtomsBcRz: return AtomsCas_AtomsBcRz(o);
        }
        return MX_NO;
    case MXOP_B2r:
        switch (f) {
        case MXF_Dest: return B2r_Dest(o);
        case MXF_DestPred: return B2r_DestPred(o);
        case MXF_SrcA: return B2r_SrcA(o);
        case MXF_Pred: return B2r_Pred(o);
        case MXF_PredInv: return B2r_PredInv(o);
        case MXF_Mode: return B2r_Mode(o);
        }
        return MX_NO;
    case MXOP_Bar:
        switch (f) {
        case MXF_SrcA: return Bar_SrcA(o);
        case MXF_Imm12: return Bar_Imm12(o);
        case MXF_Pred: return Bar_Pred(o);
        case MXF_PredInv: return Bar_PredInv(o);
        case MXF_SrcPredInv: return Bar_SrcPredInv(o);
        case MXF_SrcPred: return Bar_SrcPred(o);
        case MXF_BarOp: return Bar_BarOp(o);
        case MXF_BarRedOp: return Bar_BarRedOp(o);
        case MXF_AFixBar: return Bar_AFixBar(o);
        case MXF_BFixBar: return Bar_BFixBar(o);
        }
        return MX_NO;
    case MXOP_BfeC:
        switch (f) {
        case MXF_Dest: return BfeC_Dest(o);
        case MXF_SrcA: return BfeC_SrcA(o);
        case MXF_CbufSlot: return BfeC_CbufSlot(o);
        case MXF_CbufOffset: return BfeC_CbufOffset(o);
        case MXF_Pred: return BfeC_Pred(o);
        case MXF_PredInv: return BfeC_PredInv(o);
        case MXF_WriteCC: return BfeC_WriteCC(o);
        case MXF_Signed: return BfeC_Signed(o);
        case MXF_Brev: return BfeC_Brev(o);
        }
        return MX_NO;
    case MXOP_BfeI:
        switch (f) {
        case MXF_Dest: return BfeI_Dest(o);
        case MXF_SrcA: return BfeI_SrcA(o);
        case MXF_Imm20: return BfeI_Imm20(o);
        case MXF_Pred: return BfeI_Pred(o);
        case MXF_PredInv: return BfeI_PredInv(o);
        case MXF_WriteCC: return BfeI_WriteCC(o);
        case MXF_Signed: return BfeI_Signed(o);
        case MXF_Brev: return BfeI_Brev(o);
        }
        return MX_NO;
    case MXOP_BfeR:
        switch (f) {
        case MXF_Dest: return BfeR_Dest(o);
        case MXF_SrcA: return BfeR_SrcA(o);
        case MXF_SrcB: return BfeR_SrcB(o);
        case MXF_Pred: return BfeR_Pred(o);
        case MXF_PredInv: return BfeR_PredInv(o);
        case MXF_WriteCC: return BfeR_WriteCC(o);
        case MXF_Signed: return BfeR_Signed(o);
        case MXF_Brev: return BfeR_Brev(o);
        }
        return MX_NO;
    case MXOP_BfiC:
        switch (f) {
        case MXF_Dest: return BfiC_Dest(o);
        case MXF_SrcA: return BfiC_SrcA(o);
        case MXF_CbufSlot: return BfiC_CbufSlot(o);
        case MXF_CbufOffset: return BfiC_CbufOffset(o);
        case MXF_SrcC: return BfiC_SrcC(o);
        case MXF_Pred: return BfiC_Pred(o);
        case MXF_PredInv: return BfiC_PredInv(o);
        case MXF_WriteCC: return BfiC_WriteCC(o);
        }
        return MX_NO;
    case MXOP_BfiI:
        switch (f) {
        case MXF_Dest: return BfiI_Dest(o);
        case MXF_SrcA: return BfiI_SrcA(o);
        case MXF_Imm20: return BfiI_Imm20(o);
        case MXF_SrcC: return BfiI_SrcC(o);
        case MXF_Pred: return BfiI_Pred(o);
        case MXF_PredInv: return BfiI_PredInv(o);
        case MXF_WriteCC: return BfiI_WriteCC(o);
        }
        return MX_NO;
    case MXOP_BfiR:
        switch (f) {
        case MXF_Dest: return BfiR_Dest(o);
        case MXF_SrcA: return BfiR_SrcA(o);
        case MXF_SrcB: return BfiR_SrcB(o);
        case MXF_SrcC: return BfiR_SrcC(o);
        case MXF_Pred: return BfiR_Pred(o);
        case MXF_PredInv: return BfiR_PredInv(o);
        case MXF_WriteCC: return BfiR_WriteCC(o);
        }
        return MX_NO;
    case MXOP_BfiRc:
        switch (f) {
        case MXF_Dest: return BfiRc_Dest(o);
        case MXF_SrcA: return BfiRc_SrcA(o);
        case MXF_SrcC: return BfiRc_SrcC(o);
        case MXF_CbufSlot: return BfiRc_CbufSlot(o);
        case MXF_CbufOffset: return BfiRc_CbufOffset(o);
        case MXF_Pred: return BfiRc_Pred(o);
        case MXF_PredInv: return BfiRc_PredInv(o);
        case MXF_WriteCC: return BfiRc_WriteCC(o);
        }
        return MX_NO;
    case MXOP_Bpt:
        switch (f) {
        case MXF_Imm20: return Bpt_Imm20(o);
        case MXF_Bpt: return Bpt_Bpt(o);
        }
        return MX_NO;
    case MXOP_Bra:
        switch (f) {
        case MXF_Pred: return Bra_Pred(o);
        case MXF_PredInv: return Bra_PredInv(o);
        case MXF_Ccc: return Bra_Ccc(o);
        case MXF_Imm24: return Bra_Imm24(o);
        case MXF_Ca: return Bra_Ca(o);
        case MXF_Lmt: return Bra_Lmt(o);
        case MXF_U: return Bra_U(o);
        }
        return MX_NO;
    case MXOP_Brk:
        switch (f) {
        case MXF_Pred: return Brk_Pred(o);
        case MXF_PredInv: return Brk_PredInv(o);
        case MXF_Ccc: return Brk_Ccc(o);
        }
        return MX_NO;
    case MXOP_Brx:
        switch (f) {
        case MXF_SrcA: return Brx_SrcA(o);
        case MXF_Pred: return Brx_Pred(o);
        case MXF_PredInv: return Brx_PredInv(o);
        case MXF_Ccc: return Brx_Ccc(o);
        case MXF_Imm24: return Brx_Imm24(o);
        case MXF_Ca: return Brx_Ca(o);
        case MXF_Lmt: return Brx_Lmt(o);
        }
        return MX_NO;
    case MXOP_Cal:
        switch (f) {
        case MXF_Ca: return Cal_Ca(o);
        case MXF_Imm24: return Cal_Imm24(o);
        case MXF_Inc: return Cal_Inc(o);
        }
        return MX_NO;
    case MXOP_Cctl:
        switch (f) {
        case MXF_SrcA: return Cctl_SrcA(o);
        case MXF_Pred: return Cctl_Pred(o);
        case MXF_PredInv: return Cctl_PredInv(o);
        case MXF_Imm30: return Cctl_Imm30(o);
        case MXF_E: return Cctl_E(o);
        case MXF_Cache: return Cctl_Cache(o);
        case MXF_CctlOp: return Cctl_CctlOp(o);
        }
        return MX_NO;
    case MXOP_Cctll:
        switch (f) {
        case MXF_SrcA: return Cctll_SrcA(o);
        case MXF_Pred: return Cctll_Pred(o);
        case MXF_PredInv: return Cctll_PredInv(o);
        case MXF_Imm22: return Cctll_Imm22(o);
        case MXF_Cache: return Cctll_Cache(o);
        case MXF_CctlOp: return Cctll_CctlOp(o);
        }
        return MX_NO;
    case MXOP_Cctlt:
        switch (f) {
        case MXF_Pred: return Cctlt_Pred(o);
        case MXF_PredInv: return Cctlt_PredInv(o);
        case MXF_TsIdx13: return Cctlt_TsIdx13(o);
        case MXF_CctltOp: return Cctlt_CctltOp(o);
        }
        return MX_NO;
    case MXOP_Cont:
        switch (f) {
        case MXF_Pred: return Cont_Pred(o);
        case MXF_PredInv: return Cont_PredInv(o);
        case MXF_Ccc: return Cont_Ccc(o);
        }
        return MX_NO;
    case MXOP_Cs2r:
        switch (f) {
        case MXF_Dest: return Cs2r_Dest(o);
        case MXF_Pred: return Cs2r_Pred(o);
        case MXF_PredInv: return Cs2r_PredInv(o);
        case MXF_SReg: return Cs2r_SReg(o);
        }
        return MX_NO;
    case MXOP_Cset:
        switch (f) {
        case MXF_Dest: return Cset_Dest(o);
        case MXF_Pred: return Cset_Pred(o);
        case MXF_PredInv: return Cset_PredInv(o);
        case MXF_WriteCC: return Cset_WriteCC(o);
        case MXF_Ccc: return Cset_Ccc(o);
        case MXF_SrcPred: return Cset_SrcPred(o);
        case MXF_SrcPredInv: return Cset_SrcPredInv(o);
        case MXF_BVal: return Cset_BVal(o);
        case MXF_Bop: return Cset_Bop(o);
        }
        return MX_NO;
    case MXOP_Csetp:
        switch (f) {
        case MXF_Pred: return Csetp_Pred(o);
        case MXF_PredInv: return Csetp_PredInv(o);
        case MXF_WriteCC: return Csetp_WriteCC(o);
        case MXF_Ccc: return Csetp_Ccc(o);
        case MXF_DestPred: return Csetp_DestPred(o);
        case MXF_DestPredInv: return Csetp_DestPredInv(o);
        case MXF_SrcPred: return Csetp_SrcPred(o);
        case MXF_SrcPredInv: return Csetp_SrcPredInv(o);
        case MXF_Bop: return Csetp_Bop(o);
        }
        return MX_NO;
    case MXOP_DaddC:
        switch (f) {
        case MXF_Dest: return DaddC_Dest(o);
        case MXF_SrcA: return DaddC_SrcA(o);
        case MXF_CbufSlot: return DaddC_CbufSlot(o);
        case MXF_CbufOffset: return DaddC_CbufOffset(o);
        case MXF_Pred: return DaddC_Pred(o);
        case MXF_PredInv: return DaddC_PredInv(o);
        case MXF_WriteCC: return DaddC_WriteCC(o);
        case MXF_AbsB: return DaddC_AbsB(o);
        case MXF_NegA: return DaddC_NegA(o);
        case MXF_AbsA: return DaddC_AbsA(o);
        case MXF_NegB: return DaddC_NegB(o);
        case MXF_RoundMode: return DaddC_RoundMode(o);
        }
        return MX_NO;
    case MXOP_DaddI:
        switch (f) {
        case MXF_Dest: return DaddI_Dest(o);
        case MXF_SrcA: return DaddI_SrcA(o);
        case MXF_Imm20: return DaddI_Imm20(o);
        case MXF_Pred: return DaddI_Pred(o);
        case MXF_PredInv: return DaddI_PredInv(o);
        case MXF_WriteCC: return DaddI_WriteCC(o);
        case MXF_AbsB: return DaddI_AbsB(o);
        case MXF_NegA: return DaddI_NegA(o);
        case MXF_AbsA: return DaddI_AbsA(o);
        case MXF_NegB: return DaddI_NegB(o);
        case MXF_RoundMode: return DaddI_RoundMode(o);
        }
        return MX_NO;
    case MXOP_DaddR:
        switch (f) {
        case MXF_Dest: return DaddR_Dest(o);
        case MXF_SrcA: return DaddR_SrcA(o);
        case MXF_SrcB: return DaddR_SrcB(o);
        case MXF_Pred: return DaddR_Pred(o);
        case MXF_PredInv: return DaddR_PredInv(o);
        case MXF_WriteCC: return DaddR_WriteCC(o);
        case MXF_AbsB: return DaddR_AbsB(o);
        case MXF_NegA: return DaddR_NegA(o);
        case MXF_AbsA: return DaddR_AbsA(o);
        case MXF_NegB: return DaddR_NegB(o);
        case MXF_RoundMode: return DaddR_RoundMode(o);
        }
        return MX_NO;
    case MXOP_Depbar:
        switch (f) {
        case MXF_Pred: return Depbar_Pred(o);
        case MXF_PredInv: return Depbar_PredInv(o);
        case MXF_Le: return Depbar_Le(o);
        case MXF_Sbid: return Depbar_Sbid(o);
        case MXF_PendCnt: return Depbar_PendCnt(o);
        case MXF_Imm6: return Depbar_Imm6(o);
        }
        return MX_NO;
    case MXOP_DfmaC:
        switch (f) {
        case MXF_Dest: return DfmaC_Dest(o);
        case MXF_SrcA: return DfmaC_SrcA(o);
        case MXF_CbufSlot: return DfmaC_CbufSlot(o);
        case MXF_CbufOffset: return DfmaC_CbufOffset(o);
        case MXF_SrcC: return DfmaC_SrcC(o);
        case MXF_Pred: return DfmaC_Pred(o);
        case MXF_PredInv: return DfmaC_PredInv(o);
        case MXF_WriteCC: return DfmaC_WriteCC(o);
        case MXF_RoundMode: return DfmaC_RoundMode(o);
        case MXF_NegC: return DfmaC_NegC(o);
        case MXF_NegA: return DfmaC_NegA(o);
        }
        return MX_NO;
    case MXOP_DfmaI:
        switch (f) {
        case MXF_Dest: return DfmaI_Dest(o);
        case MXF_SrcA: return DfmaI_SrcA(o);
        case MXF_Imm20: return DfmaI_Imm20(o);
        case MXF_SrcC: return DfmaI_SrcC(o);
        case MXF_Pred: return DfmaI_Pred(o);
        case MXF_PredInv: return DfmaI_PredInv(o);
        case MXF_WriteCC: return DfmaI_WriteCC(o);
        case MXF_RoundMode: return DfmaI_RoundMode(o);
        case MXF_NegC: return DfmaI_NegC(o);
        case MXF_NegA: return DfmaI_NegA(o);
        }
        return MX_NO;
    case MXOP_DfmaR:
        switch (f) {
        case MXF_Dest: return DfmaR_Dest(o);
        case MXF_SrcA: return DfmaR_SrcA(o);
        case MXF_SrcB: return DfmaR_SrcB(o);
        case MXF_SrcC: return DfmaR_SrcC(o);
        case MXF_Pred: return DfmaR_Pred(o);
        case MXF_PredInv: return DfmaR_PredInv(o);
        case MXF_WriteCC: return DfmaR_WriteCC(o);
        case MXF_RoundMode: return DfmaR_RoundMode(o);
        case MXF_NegC: return DfmaR_NegC(o);
        case MXF_NegA: return DfmaR_NegA(o);
        }
        return MX_NO;
    case MXOP_DfmaRc:
        switch (f) {
        case MXF_Dest: return DfmaRc_Dest(o);
        case MXF_SrcA: return DfmaRc_SrcA(o);
        case MXF_SrcC: return DfmaRc_SrcC(o);
        case MXF_CbufSlot: return DfmaRc_CbufSlot(o);
        case MXF_CbufOffset: return DfmaRc_CbufOffset(o);
        case MXF_Pred: return DfmaRc_Pred(o);
        case MXF_PredInv: return DfmaRc_PredInv(o);
        case MXF_WriteCC: return DfmaRc_WriteCC(o);
        case MXF_RoundMode: return DfmaRc_RoundMode(o);
        case MXF_NegC: return DfmaRc_NegC(o);
        case MXF_NegA: return DfmaRc_NegA(o);
        }
        return MX_NO;
    case MXOP_DmnmxC:
        switch (f) {
        case MXF_Dest: return DmnmxC_Dest(o);
        case MXF_SrcA: return DmnmxC_SrcA(o);
        case MXF_CbufSlot: return DmnmxC_CbufSlot(o);
        case MXF_CbufOffset: return DmnmxC_CbufOffset(o);
        case MXF_Pred: return DmnmxC_Pred(o);
        case MXF_PredInv: return DmnmxC_PredInv(o);
        case MXF_WriteCC: return DmnmxC_WriteCC(o);
        case MXF_AbsB: return DmnmxC_AbsB(o);
        case MXF_NegA: return DmnmxC_NegA(o);
        case MXF_AbsA: return DmnmxC_AbsA(o);
        case MXF_NegB: return DmnmxC_NegB(o);
        case MXF_SrcPred: return DmnmxC_SrcPred(o);
        case MXF_SrcPredInv: return DmnmxC_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_DmnmxI:
        switch (f) {
        case MXF_Dest: return DmnmxI_Dest(o);
        case MXF_SrcA: return DmnmxI_SrcA(o);
        case MXF_Imm20: return DmnmxI_Imm20(o);
        case MXF_Pred: return DmnmxI_Pred(o);
        case MXF_PredInv: return DmnmxI_PredInv(o);
        case MXF_WriteCC: return DmnmxI_WriteCC(o);
        case MXF_AbsB: return DmnmxI_AbsB(o);
        case MXF_NegA: return DmnmxI_NegA(o);
        case MXF_AbsA: return DmnmxI_AbsA(o);
        case MXF_NegB: return DmnmxI_NegB(o);
        case MXF_SrcPred: return DmnmxI_SrcPred(o);
        case MXF_SrcPredInv: return DmnmxI_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_DmnmxR:
        switch (f) {
        case MXF_Dest: return DmnmxR_Dest(o);
        case MXF_SrcA: return DmnmxR_SrcA(o);
        case MXF_SrcB: return DmnmxR_SrcB(o);
        case MXF_Pred: return DmnmxR_Pred(o);
        case MXF_PredInv: return DmnmxR_PredInv(o);
        case MXF_WriteCC: return DmnmxR_WriteCC(o);
        case MXF_AbsB: return DmnmxR_AbsB(o);
        case MXF_NegA: return DmnmxR_NegA(o);
        case MXF_AbsA: return DmnmxR_AbsA(o);
        case MXF_NegB: return DmnmxR_NegB(o);
        case MXF_SrcPred: return DmnmxR_SrcPred(o);
        case MXF_SrcPredInv: return DmnmxR_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_DmulC:
        switch (f) {
        case MXF_Dest: return DmulC_Dest(o);
        case MXF_SrcA: return DmulC_SrcA(o);
        case MXF_CbufSlot: return DmulC_CbufSlot(o);
        case MXF_CbufOffset: return DmulC_CbufOffset(o);
        case MXF_Pred: return DmulC_Pred(o);
        case MXF_PredInv: return DmulC_PredInv(o);
        case MXF_WriteCC: return DmulC_WriteCC(o);
        case MXF_RoundMode: return DmulC_RoundMode(o);
        case MXF_NegA: return DmulC_NegA(o);
        }
        return MX_NO;
    case MXOP_DmulI:
        switch (f) {
        case MXF_Dest: return DmulI_Dest(o);
        case MXF_SrcA: return DmulI_SrcA(o);
        case MXF_Imm20: return DmulI_Imm20(o);
        case MXF_Pred: return DmulI_Pred(o);
        case MXF_PredInv: return DmulI_PredInv(o);
        case MXF_WriteCC: return DmulI_WriteCC(o);
        case MXF_RoundMode: return DmulI_RoundMode(o);
        case MXF_NegA: return DmulI_NegA(o);
        }
        return MX_NO;
    case MXOP_DmulR:
        switch (f) {
        case MXF_Dest: return DmulR_Dest(o);
        case MXF_SrcA: return DmulR_SrcA(o);
        case MXF_SrcB: return DmulR_SrcB(o);
        case MXF_Pred: return DmulR_Pred(o);
        case MXF_PredInv: return DmulR_PredInv(o);
        case MXF_WriteCC: return DmulR_WriteCC(o);
        case MXF_RoundMode: return DmulR_RoundMode(o);
        case MXF_NegA: return DmulR_NegA(o);
        }
        return MX_NO;
    case MXOP_DsetC:
        switch (f) {
        case MXF_Dest: return DsetC_Dest(o);
        case MXF_SrcA: return DsetC_SrcA(o);
        case MXF_CbufSlot: return DsetC_CbufSlot(o);
        case MXF_CbufOffset: return DsetC_CbufOffset(o);
        case MXF_Pred: return DsetC_Pred(o);
        case MXF_PredInv: return DsetC_PredInv(o);
        case MXF_WriteCC: return DsetC_WriteCC(o);
        case MXF_AbsA: return DsetC_AbsA(o);
        case MXF_NegB: return DsetC_NegB(o);
        case MXF_BVal: return DsetC_BVal(o);
        case MXF_FComp: return DsetC_FComp(o);
        case MXF_Bop: return DsetC_Bop(o);
        case MXF_AbsB: return DsetC_AbsB(o);
        case MXF_NegA: return DsetC_NegA(o);
        case MXF_SrcPred: return DsetC_SrcPred(o);
        case MXF_SrcPredInv: return DsetC_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_DsetI:
        switch (f) {
        case MXF_Dest: return DsetI_Dest(o);
        case MXF_SrcA: return DsetI_SrcA(o);
        case MXF_Imm20: return DsetI_Imm20(o);
        case MXF_Pred: return DsetI_Pred(o);
        case MXF_PredInv: return DsetI_PredInv(o);
        case MXF_WriteCC: return DsetI_WriteCC(o);
        case MXF_AbsA: return DsetI_AbsA(o);
        case MXF_NegB: return DsetI_NegB(o);
        case MXF_BVal: return DsetI_BVal(o);
        case MXF_FComp: return DsetI_FComp(o);
        case MXF_Bop: return DsetI_Bop(o);
        case MXF_AbsB: return DsetI_AbsB(o);
        case MXF_NegA: return DsetI_NegA(o);
        case MXF_SrcPred: return DsetI_SrcPred(o);
        case MXF_SrcPredInv: return DsetI_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_DsetR:
        switch (f) {
        case MXF_Dest: return DsetR_Dest(o);
        case MXF_SrcA: return DsetR_SrcA(o);
        case MXF_SrcB: return DsetR_SrcB(o);
        case MXF_Pred: return DsetR_Pred(o);
        case MXF_PredInv: return DsetR_PredInv(o);
        case MXF_WriteCC: return DsetR_WriteCC(o);
        case MXF_AbsA: return DsetR_AbsA(o);
        case MXF_NegB: return DsetR_NegB(o);
        case MXF_BVal: return DsetR_BVal(o);
        case MXF_FComp: return DsetR_FComp(o);
        case MXF_Bop: return DsetR_Bop(o);
        case MXF_AbsB: return DsetR_AbsB(o);
        case MXF_NegA: return DsetR_NegA(o);
        case MXF_SrcPred: return DsetR_SrcPred(o);
        case MXF_SrcPredInv: return DsetR_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_DsetpC:
        switch (f) {
        case MXF_SrcA: return DsetpC_SrcA(o);
        case MXF_CbufSlot: return DsetpC_CbufSlot(o);
        case MXF_CbufOffset: return DsetpC_CbufOffset(o);
        case MXF_Pred: return DsetpC_Pred(o);
        case MXF_PredInv: return DsetpC_PredInv(o);
        case MXF_FComp: return DsetpC_FComp(o);
        case MXF_Bop: return DsetpC_Bop(o);
        case MXF_AbsB: return DsetpC_AbsB(o);
        case MXF_NegA: return DsetpC_NegA(o);
        case MXF_SrcPredInv: return DsetpC_SrcPredInv(o);
        case MXF_SrcPred: return DsetpC_SrcPred(o);
        case MXF_AbsA: return DsetpC_AbsA(o);
        case MXF_NegB: return DsetpC_NegB(o);
        case MXF_DestPred: return DsetpC_DestPred(o);
        case MXF_DestPredInv: return DsetpC_DestPredInv(o);
        }
        return MX_NO;
    case MXOP_DsetpI:
        switch (f) {
        case MXF_SrcA: return DsetpI_SrcA(o);
        case MXF_Imm20: return DsetpI_Imm20(o);
        case MXF_Pred: return DsetpI_Pred(o);
        case MXF_PredInv: return DsetpI_PredInv(o);
        case MXF_FComp: return DsetpI_FComp(o);
        case MXF_Bop: return DsetpI_Bop(o);
        case MXF_AbsB: return DsetpI_AbsB(o);
        case MXF_NegA: return DsetpI_NegA(o);
        case MXF_SrcPredInv: return DsetpI_SrcPredInv(o);
        case MXF_SrcPred: return DsetpI_SrcPred(o);
        case MXF_AbsA: return DsetpI_AbsA(o);
        case MXF_NegB: return DsetpI_NegB(o);
        case MXF_DestPred: return DsetpI_DestPred(o);
        case MXF_DestPredInv: return DsetpI_DestPredInv(o);
        }
        return MX_NO;
    case MXOP_DsetpR:
        switch (f) {
        case MXF_SrcA: return DsetpR_SrcA(o);
        case MXF_SrcB: return DsetpR_SrcB(o);
        case MXF_Pred: return DsetpR_Pred(o);
        case MXF_PredInv: return DsetpR_PredInv(o);
        case MXF_FComp: return DsetpR_FComp(o);
        case MXF_Bop: return DsetpR_Bop(o);
        case MXF_AbsB: return DsetpR_AbsB(o);
        case MXF_NegA: return DsetpR_NegA(o);
        case MXF_SrcPredInv: return DsetpR_SrcPredInv(o);
        case MXF_SrcPred: return DsetpR_SrcPred(o);
        case MXF_AbsA: return DsetpR_AbsA(o);
        case MXF_NegB: return DsetpR_NegB(o);
        case MXF_DestPred: return DsetpR_DestPred(o);
        case MXF_DestPredInv: return DsetpR_DestPredInv(o);
        }
        return MX_NO;
    case MXOP_Exit:
        switch (f) {
        case MXF_Pred: return Exit_Pred(o);
        case MXF_PredInv: return Exit_PredInv(o);
        case MXF_Ccc: return Exit_Ccc(o);
        case MXF_KeepRefCnt: return Exit_KeepRefCnt(o);
        }
        return MX_NO;
    case MXOP_F2fC:
        switch (f) {
        case MXF_Dest: return F2fC_Dest(o);
        case MXF_CbufSlot: return F2fC_CbufSlot(o);
        case MXF_CbufOffset: return F2fC_CbufOffset(o);
        case MXF_Pred: return F2fC_Pred(o);
        case MXF_PredInv: return F2fC_PredInv(o);
        case MXF_WriteCC: return F2fC_WriteCC(o);
        case MXF_AbsB: return F2fC_AbsB(o);
        case MXF_NegB: return F2fC_NegB(o);
        case MXF_Ftz: return F2fC_Ftz(o);
        case MXF_DstFmt: return F2fC_DstFmt(o);
        case MXF_SrcFmt: return F2fC_SrcFmt(o);
        case MXF_RoundMode: return F2fC_RoundMode(o);
        case MXF_Sh: return F2fC_Sh(o);
        case MXF_Sat: return F2fC_Sat(o);
        }
        return MX_NO;
    case MXOP_F2fI:
        switch (f) {
        case MXF_Dest: return F2fI_Dest(o);
        case MXF_Imm20: return F2fI_Imm20(o);
        case MXF_Pred: return F2fI_Pred(o);
        case MXF_PredInv: return F2fI_PredInv(o);
        case MXF_WriteCC: return F2fI_WriteCC(o);
        case MXF_AbsB: return F2fI_AbsB(o);
        case MXF_NegB: return F2fI_NegB(o);
        case MXF_Ftz: return F2fI_Ftz(o);
        case MXF_DstFmt: return F2fI_DstFmt(o);
        case MXF_SrcFmt: return F2fI_SrcFmt(o);
        case MXF_RoundMode: return F2fI_RoundMode(o);
        case MXF_Sh: return F2fI_Sh(o);
        case MXF_Sat: return F2fI_Sat(o);
        }
        return MX_NO;
    case MXOP_F2fR:
        switch (f) {
        case MXF_Dest: return F2fR_Dest(o);
        case MXF_SrcB: return F2fR_SrcB(o);
        case MXF_Pred: return F2fR_Pred(o);
        case MXF_PredInv: return F2fR_PredInv(o);
        case MXF_WriteCC: return F2fR_WriteCC(o);
        case MXF_AbsB: return F2fR_AbsB(o);
        case MXF_NegB: return F2fR_NegB(o);
        case MXF_Ftz: return F2fR_Ftz(o);
        case MXF_DstFmt: return F2fR_DstFmt(o);
        case MXF_SrcFmt: return F2fR_SrcFmt(o);
        case MXF_RoundMode: return F2fR_RoundMode(o);
        case MXF_Sh: return F2fR_Sh(o);
        case MXF_Sat: return F2fR_Sat(o);
        }
        return MX_NO;
    case MXOP_F2iC:
        switch (f) {
        case MXF_Dest: return F2iC_Dest(o);
        case MXF_CbufSlot: return F2iC_CbufSlot(o);
        case MXF_CbufOffset: return F2iC_CbufOffset(o);
        case MXF_Pred: return F2iC_Pred(o);
        case MXF_PredInv: return F2iC_PredInv(o);
        case MXF_WriteCC: return F2iC_WriteCC(o);
        case MXF_AbsB: return F2iC_AbsB(o);
        case MXF_NegB: return F2iC_NegB(o);
        case MXF_Ftz: return F2iC_Ftz(o);
        case MXF_Sh: return F2iC_Sh(o);
        case MXF_IDstFmt: return F2iC_IDstFmt(o);
        case MXF_SrcFmt: return F2iC_SrcFmt(o);
        case MXF_RoundMode: return F2iC_RoundMode(o);
        }
        return MX_NO;
    case MXOP_F2iI:
        switch (f) {
        case MXF_Dest: return F2iI_Dest(o);
        case MXF_Imm20: return F2iI_Imm20(o);
        case MXF_Pred: return F2iI_Pred(o);
        case MXF_PredInv: return F2iI_PredInv(o);
        case MXF_WriteCC: return F2iI_WriteCC(o);
        case MXF_AbsB: return F2iI_AbsB(o);
        case MXF_NegB: return F2iI_NegB(o);
        case MXF_Ftz: return F2iI_Ftz(o);
        case MXF_Sh: return F2iI_Sh(o);
        case MXF_IDstFmt: return F2iI_IDstFmt(o);
        case MXF_SrcFmt: return F2iI_SrcFmt(o);
        case MXF_RoundMode: return F2iI_RoundMode(o);
        }
        return MX_NO;
    case MXOP_F2iR:
        switch (f) {
        case MXF_Dest: return F2iR_Dest(o);
        case MXF_SrcB: return F2iR_SrcB(o);
        case MXF_Pred: return F2iR_Pred(o);
        case MXF_PredInv: return F2iR_PredInv(o);
        case MXF_WriteCC: return F2iR_WriteCC(o);
        case MXF_AbsB: return F2iR_AbsB(o);
        case MXF_NegB: return F2iR_NegB(o);
        case MXF_Ftz: return F2iR_Ftz(o);
        case MXF_Sh: return F2iR_Sh(o);
        case MXF_IDstFmt: return F2iR_IDstFmt(o);
        case MXF_SrcFmt: return F2iR_SrcFmt(o);
        case MXF_RoundMode: return F2iR_RoundMode(o);
        }
        return MX_NO;
    case MXOP_Fadd32i:
        switch (f) {
        case MXF_Dest: return Fadd32i_Dest(o);
        case MXF_SrcA: return Fadd32i_SrcA(o);
        case MXF_Pred: return Fadd32i_Pred(o);
        case MXF_PredInv: return Fadd32i_PredInv(o);
        case MXF_WriteCC: return Fadd32i_WriteCC(o);
        case MXF_Imm32: return Fadd32i_Imm32(o);
        case MXF_AbsB: return Fadd32i_AbsB(o);
        case MXF_NegA: return Fadd32i_NegA(o);
        case MXF_Ftz: return Fadd32i_Ftz(o);
        case MXF_AbsA: return Fadd32i_AbsA(o);
        case MXF_NegB: return Fadd32i_NegB(o);
        }
        return MX_NO;
    case MXOP_FaddC:
        switch (f) {
        case MXF_Dest: return FaddC_Dest(o);
        case MXF_SrcA: return FaddC_SrcA(o);
        case MXF_CbufSlot: return FaddC_CbufSlot(o);
        case MXF_CbufOffset: return FaddC_CbufOffset(o);
        case MXF_Pred: return FaddC_Pred(o);
        case MXF_PredInv: return FaddC_PredInv(o);
        case MXF_WriteCC: return FaddC_WriteCC(o);
        case MXF_Sat: return FaddC_Sat(o);
        case MXF_AbsB: return FaddC_AbsB(o);
        case MXF_NegA: return FaddC_NegA(o);
        case MXF_AbsA: return FaddC_AbsA(o);
        case MXF_NegB: return FaddC_NegB(o);
        case MXF_Ftz: return FaddC_Ftz(o);
        case MXF_RoundMode: return FaddC_RoundMode(o);
        }
        return MX_NO;
    case MXOP_FaddI:
        switch (f) {
        case MXF_Dest: return FaddI_Dest(o);
        case MXF_SrcA: return FaddI_SrcA(o);
        case MXF_Imm20: return FaddI_Imm20(o);
        case MXF_Pred: return FaddI_Pred(o);
        case MXF_PredInv: return FaddI_PredInv(o);
        case MXF_WriteCC: return FaddI_WriteCC(o);
        case MXF_Sat: return FaddI_Sat(o);
        case MXF_AbsB: return FaddI_AbsB(o);
        case MXF_NegA: return FaddI_NegA(o);
        case MXF_AbsA: return FaddI_AbsA(o);
        case MXF_NegB: return FaddI_NegB(o);
        case MXF_Ftz: return FaddI_Ftz(o);
        case MXF_RoundMode: return FaddI_RoundMode(o);
        }
        return MX_NO;
    case MXOP_FaddR:
        switch (f) {
        case MXF_Dest: return FaddR_Dest(o);
        case MXF_SrcA: return FaddR_SrcA(o);
        case MXF_SrcB: return FaddR_SrcB(o);
        case MXF_Pred: return FaddR_Pred(o);
        case MXF_PredInv: return FaddR_PredInv(o);
        case MXF_WriteCC: return FaddR_WriteCC(o);
        case MXF_Sat: return FaddR_Sat(o);
        case MXF_AbsB: return FaddR_AbsB(o);
        case MXF_NegA: return FaddR_NegA(o);
        case MXF_AbsA: return FaddR_AbsA(o);
        case MXF_NegB: return FaddR_NegB(o);
        case MXF_Ftz: return FaddR_Ftz(o);
        case MXF_RoundMode: return FaddR_RoundMode(o);
        }
        return MX_NO;
    case MXOP_FchkC:
        switch (f) {
        case MXF_DestPred: return FchkC_DestPred(o);
        case MXF_SrcA: return FchkC_SrcA(o);
        case MXF_CbufSlot: return FchkC_CbufSlot(o);
        case MXF_CbufOffset: return FchkC_CbufOffset(o);
        case MXF_Pred: return FchkC_Pred(o);
        case MXF_PredInv: return FchkC_PredInv(o);
        case MXF_AbsB: return FchkC_AbsB(o);
        case MXF_NegA: return FchkC_NegA(o);
        case MXF_AbsA: return FchkC_AbsA(o);
        case MXF_NegB: return FchkC_NegB(o);
        case MXF_ChkModeF: return FchkC_ChkModeF(o);
        }
        return MX_NO;
    case MXOP_FchkI:
        switch (f) {
        case MXF_DestPred: return FchkI_DestPred(o);
        case MXF_SrcA: return FchkI_SrcA(o);
        case MXF_Imm20: return FchkI_Imm20(o);
        case MXF_Pred: return FchkI_Pred(o);
        case MXF_PredInv: return FchkI_PredInv(o);
        case MXF_AbsB: return FchkI_AbsB(o);
        case MXF_NegA: return FchkI_NegA(o);
        case MXF_AbsA: return FchkI_AbsA(o);
        case MXF_NegB: return FchkI_NegB(o);
        case MXF_ChkModeF: return FchkI_ChkModeF(o);
        }
        return MX_NO;
    case MXOP_FchkR:
        switch (f) {
        case MXF_DestPred: return FchkR_DestPred(o);
        case MXF_SrcA: return FchkR_SrcA(o);
        case MXF_SrcB: return FchkR_SrcB(o);
        case MXF_Pred: return FchkR_Pred(o);
        case MXF_PredInv: return FchkR_PredInv(o);
        case MXF_AbsB: return FchkR_AbsB(o);
        case MXF_NegA: return FchkR_NegA(o);
        case MXF_AbsA: return FchkR_AbsA(o);
        case MXF_NegB: return FchkR_NegB(o);
        case MXF_ChkModeF: return FchkR_ChkModeF(o);
        }
        return MX_NO;
    case MXOP_FcmpC:
        switch (f) {
        case MXF_Dest: return FcmpC_Dest(o);
        case MXF_SrcA: return FcmpC_SrcA(o);
        case MXF_CbufSlot: return FcmpC_CbufSlot(o);
        case MXF_CbufOffset: return FcmpC_CbufOffset(o);
        case MXF_SrcC: return FcmpC_SrcC(o);
        case MXF_Pred: return FcmpC_Pred(o);
        case MXF_PredInv: return FcmpC_PredInv(o);
        case MXF_FComp: return FcmpC_FComp(o);
        case MXF_Ftz: return FcmpC_Ftz(o);
        }
        return MX_NO;
    case MXOP_FcmpI:
        switch (f) {
        case MXF_Dest: return FcmpI_Dest(o);
        case MXF_SrcA: return FcmpI_SrcA(o);
        case MXF_Imm20: return FcmpI_Imm20(o);
        case MXF_SrcC: return FcmpI_SrcC(o);
        case MXF_Pred: return FcmpI_Pred(o);
        case MXF_PredInv: return FcmpI_PredInv(o);
        case MXF_FComp: return FcmpI_FComp(o);
        case MXF_Ftz: return FcmpI_Ftz(o);
        }
        return MX_NO;
    case MXOP_FcmpR:
        switch (f) {
        case MXF_Dest: return FcmpR_Dest(o);
        case MXF_SrcA: return FcmpR_SrcA(o);
        case MXF_SrcB: return FcmpR_SrcB(o);
        case MXF_SrcC: return FcmpR_SrcC(o);
        case MXF_Pred: return FcmpR_Pred(o);
        case MXF_PredInv: return FcmpR_PredInv(o);
        case MXF_FComp: return FcmpR_FComp(o);
        case MXF_Ftz: return FcmpR_Ftz(o);
        }
        return MX_NO;
    case MXOP_FcmpRc:
        switch (f) {
        case MXF_Dest: return FcmpRc_Dest(o);
        case MXF_SrcA: return FcmpRc_SrcA(o);
        case MXF_SrcC: return FcmpRc_SrcC(o);
        case MXF_CbufSlot: return FcmpRc_CbufSlot(o);
        case MXF_CbufOffset: return FcmpRc_CbufOffset(o);
        case MXF_Pred: return FcmpRc_Pred(o);
        case MXF_PredInv: return FcmpRc_PredInv(o);
        case MXF_FComp: return FcmpRc_FComp(o);
        case MXF_Ftz: return FcmpRc_Ftz(o);
        }
        return MX_NO;
    case MXOP_Ffma32i:
        switch (f) {
        case MXF_Dest: return Ffma32i_Dest(o);
        case MXF_SrcA: return Ffma32i_SrcA(o);
        case MXF_Imm32: return Ffma32i_Imm32(o);
        case MXF_Pred: return Ffma32i_Pred(o);
        case MXF_PredInv: return Ffma32i_PredInv(o);
        case MXF_NegC: return Ffma32i_NegC(o);
        case MXF_NegA: return Ffma32i_NegA(o);
        case MXF_Sat: return Ffma32i_Sat(o);
        case MXF_WriteCC: return Ffma32i_WriteCC(o);
        case MXF_Fmz: return Ffma32i_Fmz(o);
        }
        return MX_NO;
    case MXOP_FfmaC:
        switch (f) {
        case MXF_Dest: return FfmaC_Dest(o);
        case MXF_SrcA: return FfmaC_SrcA(o);
        case MXF_CbufSlot: return FfmaC_CbufSlot(o);
        case MXF_CbufOffset: return FfmaC_CbufOffset(o);
        case MXF_SrcC: return FfmaC_SrcC(o);
        case MXF_Pred: return FfmaC_Pred(o);
        case MXF_PredInv: return FfmaC_PredInv(o);
        case MXF_WriteCC: return FfmaC_WriteCC(o);
        case MXF_NegA: return FfmaC_NegA(o);
        case MXF_NegC: return FfmaC_NegC(o);
        case MXF_Sat: return FfmaC_Sat(o);
        case MXF_RoundMode: return FfmaC_RoundMode(o);
        case MXF_Fmz: return FfmaC_Fmz(o);
        }
        return MX_NO;
    case MXOP_FfmaI:
        switch (f) {
        case MXF_Dest: return FfmaI_Dest(o);
        case MXF_SrcA: return FfmaI_SrcA(o);
        case MXF_Imm20: return FfmaI_Imm20(o);
        case MXF_SrcC: return FfmaI_SrcC(o);
        case MXF_Pred: return FfmaI_Pred(o);
        case MXF_PredInv: return FfmaI_PredInv(o);
        case MXF_WriteCC: return FfmaI_WriteCC(o);
        case MXF_NegA: return FfmaI_NegA(o);
        case MXF_NegC: return FfmaI_NegC(o);
        case MXF_Sat: return FfmaI_Sat(o);
        case MXF_RoundMode: return FfmaI_RoundMode(o);
        case MXF_Fmz: return FfmaI_Fmz(o);
        }
        return MX_NO;
    case MXOP_FfmaR:
        switch (f) {
        case MXF_Dest: return FfmaR_Dest(o);
        case MXF_SrcA: return FfmaR_SrcA(o);
        case MXF_SrcB: return FfmaR_SrcB(o);
        case MXF_SrcC: return FfmaR_SrcC(o);
        case MXF_Pred: return FfmaR_Pred(o);
        case MXF_PredInv: return FfmaR_PredInv(o);
        case MXF_WriteCC: return FfmaR_WriteCC(o);
        case MXF_NegA: return FfmaR_NegA(o);
        case MXF_NegC: return FfmaR_NegC(o);
        case MXF_Sat: return FfmaR_Sat(o);
        case MXF_RoundMode: return FfmaR_RoundMode(o);
        case MXF_Fmz: return FfmaR_Fmz(o);
        }
        return MX_NO;
    case MXOP_FfmaRc:
        switch (f) {
        case MXF_Dest: return FfmaRc_Dest(o);
        case MXF_SrcA: return FfmaRc_SrcA(o);
        case MXF_SrcC: return FfmaRc_SrcC(o);
        case MXF_CbufSlot: return FfmaRc_CbufSlot(o);
        case MXF_CbufOffset: return FfmaRc_CbufOffset(o);
        case MXF_Pred: return FfmaRc_Pred(o);
        case MXF_PredInv: return FfmaRc_PredInv(o);
        case MXF_WriteCC: return FfmaRc_WriteCC(o);
        case MXF_NegA: return FfmaRc_NegA(o);
        case MXF_NegC: return FfmaRc_NegC(o);
        case MXF_Sat: return FfmaRc_Sat(o);
        case MXF_RoundMode: return FfmaRc_RoundMode(o);
        case MXF_Fmz: return FfmaRc_Fmz(o);
        }
        return MX_NO;
    case MXOP_FloC:
        switch (f) {
        case MXF_Dest: return FloC_Dest(o);
        case MXF_CbufSlot: return FloC_CbufSlot(o);
        case MXF_CbufOffset: return FloC_CbufOffset(o);
        case MXF_Pred: return FloC_Pred(o);
        case MXF_PredInv: return FloC_PredInv(o);
        case MXF_WriteCC: return FloC_WriteCC(o);
        case MXF_Signed: return FloC_Signed(o);
        case MXF_Sh: return FloC_Sh(o);
        case MXF_NegB: return FloC_NegB(o);
        }
        return MX_NO;
    case MXOP_FloI:
        switch (f) {
        case MXF_Dest: return FloI_Dest(o);
        case MXF_Imm20: return FloI_Imm20(o);
        case MXF_Pred: return FloI_Pred(o);
        case MXF_PredInv: return FloI_PredInv(o);
        case MXF_WriteCC: return FloI_WriteCC(o);
        case MXF_Signed: return FloI_Signed(o);
        case MXF_Sh: return FloI_Sh(o);
        case MXF_NegB: return FloI_NegB(o);
        }
        return MX_NO;
    case MXOP_FloR:
        switch (f) {
        case MXF_Dest: return FloR_Dest(o);
        case MXF_SrcB: return FloR_SrcB(o);
        case MXF_Pred: return FloR_Pred(o);
        case MXF_PredInv: return FloR_PredInv(o);
        case MXF_WriteCC: return FloR_WriteCC(o);
        case MXF_Signed: return FloR_Signed(o);
        case MXF_Sh: return FloR_Sh(o);
        case MXF_NegB: return FloR_NegB(o);
        }
        return MX_NO;
    case MXOP_FmnmxC:
        switch (f) {
        case MXF_Dest: return FmnmxC_Dest(o);
        case MXF_SrcA: return FmnmxC_SrcA(o);
        case MXF_CbufSlot: return FmnmxC_CbufSlot(o);
        case MXF_CbufOffset: return FmnmxC_CbufOffset(o);
        case MXF_Pred: return FmnmxC_Pred(o);
        case MXF_PredInv: return FmnmxC_PredInv(o);
        case MXF_WriteCC: return FmnmxC_WriteCC(o);
        case MXF_AbsB: return FmnmxC_AbsB(o);
        case MXF_NegA: return FmnmxC_NegA(o);
        case MXF_AbsA: return FmnmxC_AbsA(o);
        case MXF_NegB: return FmnmxC_NegB(o);
        case MXF_Ftz: return FmnmxC_Ftz(o);
        case MXF_SrcPred: return FmnmxC_SrcPred(o);
        case MXF_SrcPredInv: return FmnmxC_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_FmnmxI:
        switch (f) {
        case MXF_Dest: return FmnmxI_Dest(o);
        case MXF_SrcA: return FmnmxI_SrcA(o);
        case MXF_Imm20: return FmnmxI_Imm20(o);
        case MXF_Pred: return FmnmxI_Pred(o);
        case MXF_PredInv: return FmnmxI_PredInv(o);
        case MXF_WriteCC: return FmnmxI_WriteCC(o);
        case MXF_AbsB: return FmnmxI_AbsB(o);
        case MXF_NegA: return FmnmxI_NegA(o);
        case MXF_AbsA: return FmnmxI_AbsA(o);
        case MXF_NegB: return FmnmxI_NegB(o);
        case MXF_Ftz: return FmnmxI_Ftz(o);
        case MXF_SrcPred: return FmnmxI_SrcPred(o);
        case MXF_SrcPredInv: return FmnmxI_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_FmnmxR:
        switch (f) {
        case MXF_Dest: return FmnmxR_Dest(o);
        case MXF_SrcA: return FmnmxR_SrcA(o);
        case MXF_SrcB: return FmnmxR_SrcB(o);
        case MXF_Pred: return FmnmxR_Pred(o);
        case MXF_PredInv: return FmnmxR_PredInv(o);
        case MXF_WriteCC: return FmnmxR_WriteCC(o);
        case MXF_AbsB: return FmnmxR_AbsB(o);
        case MXF_NegA: return FmnmxR_NegA(o);
        case MXF_AbsA: return FmnmxR_AbsA(o);
        case MXF_NegB: return FmnmxR_NegB(o);
        case MXF_Ftz: return FmnmxR_Ftz(o);
        case MXF_SrcPred: return FmnmxR_SrcPred(o);
        case MXF_SrcPredInv: return FmnmxR_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_Fmul32i:
        switch (f) {
        case MXF_Dest: return Fmul32i_Dest(o);
        case MXF_SrcA: return Fmul32i_SrcA(o);
        case MXF_Pred: return Fmul32i_Pred(o);
        case MXF_PredInv: return Fmul32i_PredInv(o);
        case MXF_Imm32: return Fmul32i_Imm32(o);
        case MXF_Sat: return Fmul32i_Sat(o);
        case MXF_Fmz: return Fmul32i_Fmz(o);
        case MXF_WriteCC: return Fmul32i_WriteCC(o);
        }
        return MX_NO;
    case MXOP_FmulC:
        switch (f) {
        case MXF_Dest: return FmulC_Dest(o);
        case MXF_SrcA: return FmulC_SrcA(o);
        case MXF_CbufSlot: return FmulC_CbufSlot(o);
        case MXF_CbufOffset: return FmulC_CbufOffset(o);
        case MXF_Pred: return FmulC_Pred(o);
        case MXF_PredInv: return FmulC_PredInv(o);
        case MXF_WriteCC: return FmulC_WriteCC(o);
        case MXF_RoundMode: return FmulC_RoundMode(o);
        case MXF_Fmz: return FmulC_Fmz(o);
        case MXF_Scale: return FmulC_Scale(o);
        case MXF_NegA: return FmulC_NegA(o);
        case MXF_Sat: return FmulC_Sat(o);
        }
        return MX_NO;
    case MXOP_FmulI:
        switch (f) {
        case MXF_Dest: return FmulI_Dest(o);
        case MXF_SrcA: return FmulI_SrcA(o);
        case MXF_Imm20: return FmulI_Imm20(o);
        case MXF_Pred: return FmulI_Pred(o);
        case MXF_PredInv: return FmulI_PredInv(o);
        case MXF_WriteCC: return FmulI_WriteCC(o);
        case MXF_RoundMode: return FmulI_RoundMode(o);
        case MXF_Fmz: return FmulI_Fmz(o);
        case MXF_Scale: return FmulI_Scale(o);
        case MXF_NegA: return FmulI_NegA(o);
        case MXF_Sat: return FmulI_Sat(o);
        }
        return MX_NO;
    case MXOP_FmulR:
        switch (f) {
        case MXF_Dest: return FmulR_Dest(o);
        case MXF_SrcA: return FmulR_SrcA(o);
        case MXF_SrcB: return FmulR_SrcB(o);
        case MXF_Pred: return FmulR_Pred(o);
        case MXF_PredInv: return FmulR_PredInv(o);
        case MXF_WriteCC: return FmulR_WriteCC(o);
        case MXF_RoundMode: return FmulR_RoundMode(o);
        case MXF_Fmz: return FmulR_Fmz(o);
        case MXF_Scale: return FmulR_Scale(o);
        case MXF_NegA: return FmulR_NegA(o);
        case MXF_Sat: return FmulR_Sat(o);
        }
        return MX_NO;
    case MXOP_FsetC:
        switch (f) {
        case MXF_Dest: return FsetC_Dest(o);
        case MXF_SrcA: return FsetC_SrcA(o);
        case MXF_CbufSlot: return FsetC_CbufSlot(o);
        case MXF_CbufOffset: return FsetC_CbufOffset(o);
        case MXF_Pred: return FsetC_Pred(o);
        case MXF_PredInv: return FsetC_PredInv(o);
        case MXF_WriteCC: return FsetC_WriteCC(o);
        case MXF_NegA: return FsetC_NegA(o);
        case MXF_NegB: return FsetC_NegB(o);
        case MXF_AbsA: return FsetC_AbsA(o);
        case MXF_AbsB: return FsetC_AbsB(o);
        case MXF_FComp: return FsetC_FComp(o);
        case MXF_SrcPred: return FsetC_SrcPred(o);
        case MXF_SrcPredInv: return FsetC_SrcPredInv(o);
        case MXF_Bop: return FsetC_Bop(o);
        case MXF_Ftz: return FsetC_Ftz(o);
        case MXF_BVal: return FsetC_BVal(o);
        }
        return MX_NO;
    case MXOP_FsetI:
        switch (f) {
        case MXF_Dest: return FsetI_Dest(o);
        case MXF_SrcA: return FsetI_SrcA(o);
        case MXF_Imm20: return FsetI_Imm20(o);
        case MXF_Pred: return FsetI_Pred(o);
        case MXF_PredInv: return FsetI_PredInv(o);
        case MXF_WriteCC: return FsetI_WriteCC(o);
        case MXF_NegA: return FsetI_NegA(o);
        case MXF_NegB: return FsetI_NegB(o);
        case MXF_AbsA: return FsetI_AbsA(o);
        case MXF_AbsB: return FsetI_AbsB(o);
        case MXF_FComp: return FsetI_FComp(o);
        case MXF_SrcPred: return FsetI_SrcPred(o);
        case MXF_SrcPredInv: return FsetI_SrcPredInv(o);
        case MXF_Bop: return FsetI_Bop(o);
        case MXF_Ftz: return FsetI_Ftz(o);
        case MXF_BVal: return FsetI_BVal(o);
        }
        return MX_NO;
    case MXOP_FsetR:
        switch (f) {
        case MXF_Dest: return FsetR_Dest(o);
        case MXF_SrcA: return FsetR_SrcA(o);
        case MXF_SrcB: return FsetR_SrcB(o);
        case MXF_Pred: return FsetR_Pred(o);
        case MXF_PredInv: return FsetR_PredInv(o);
        case MXF_WriteCC: return FsetR_WriteCC(o);
        case MXF_NegA: return FsetR_NegA(o);
        case MXF_NegB: return FsetR_NegB(o);
        case MXF_AbsA: return FsetR_AbsA(o);
        case MXF_AbsB: return FsetR_AbsB(o);
        case MXF_FComp: return FsetR_FComp(o);
        case MXF_SrcPred: return FsetR_SrcPred(o);
        case MXF_SrcPredInv: return FsetR_SrcPredInv(o);
        case MXF_Bop: return FsetR_Bop(o);
        case MXF_Ftz: return FsetR_Ftz(o);
        case MXF_BVal: return FsetR_BVal(o);
        }
        return MX_NO;
    case MXOP_FsetpC:
        switch (f) {
        case MXF_DestPred: return FsetpC_DestPred(o);
        case MXF_DestPredInv: return FsetpC_DestPredInv(o);
        case MXF_SrcA: return FsetpC_SrcA(o);
        case MXF_CbufSlot: return FsetpC_CbufSlot(o);
        case MXF_CbufOffset: return FsetpC_CbufOffset(o);
        case MXF_Pred: return FsetpC_Pred(o);
        case MXF_PredInv: return FsetpC_PredInv(o);
        case MXF_WriteCC: return FsetpC_WriteCC(o);
        case MXF_NegA: return FsetpC_NegA(o);
        case MXF_NegB: return FsetpC_NegB(o);
        case MXF_AbsA: return FsetpC_AbsA(o);
        case MXF_AbsB: return FsetpC_AbsB(o);
        case MXF_FComp: return FsetpC_FComp(o);
        case MXF_SrcPred: return FsetpC_SrcPred(o);
        case MXF_SrcPredInv: return FsetpC_SrcPredInv(o);
        case MXF_Bop: return FsetpC_Bop(o);
        case MXF_Ftz: return FsetpC_Ftz(o);
        }
        return MX_NO;
    case MXOP_FsetpI:
        switch (f) {
        case MXF_DestPred: return FsetpI_DestPred(o);
        case MXF_DestPredInv: return FsetpI_DestPredInv(o);
        case MXF_SrcA: return FsetpI_SrcA(o);
        case MXF_Imm20: return FsetpI_Imm20(o);
        case MXF_Pred: return FsetpI_Pred(o);
        case MXF_PredInv: return FsetpI_PredInv(o);
        case MXF_WriteCC: return FsetpI_WriteCC(o);
        case MXF_NegA: return FsetpI_NegA(o);
        case MXF_NegB: return FsetpI_NegB(o);
        case MXF_AbsA: return FsetpI_AbsA(o);
        case MXF_AbsB: return FsetpI_AbsB(o);
        case MXF_FComp: return FsetpI_FComp(o);
        case MXF_SrcPred: return FsetpI_SrcPred(o);
        case MXF_SrcPredInv: return FsetpI_SrcPredInv(o);
        case MXF_Bop: return FsetpI_Bop(o);
        case MXF_Ftz: return FsetpI_Ftz(o);
        }
        return MX_NO;
    case MXOP_FsetpR:
        switch (f) {
        case MXF_DestPred: return FsetpR_DestPred(o);
        case MXF_DestPredInv: return FsetpR_DestPredInv(o);
        case MXF_SrcA: return FsetpR_SrcA(o);
        case MXF_SrcB: return FsetpR_SrcB(o);
        case MXF_Pred: return FsetpR_Pred(o);
        case MXF_PredInv: return FsetpR_PredInv(o);
        case MXF_WriteCC: return FsetpR_WriteCC(o);
        case MXF_NegA: return FsetpR_NegA(o);
        case MXF_NegB: return FsetpR_NegB(o);
        case MXF_AbsA: return FsetpR_AbsA(o);
        case MXF_AbsB: return FsetpR_AbsB(o);
        case MXF_FComp: return FsetpR_FComp(o);
        case MXF_SrcPred: return FsetpR_SrcPred(o);
        case MXF_SrcPredInv: return FsetpR_SrcPredInv(o);
        case MXF_Bop: return FsetpR_Bop(o);
        case MXF_Ftz: return FsetpR_Ftz(o);
        }
        return MX_NO;
    case MXOP_Fswzadd:
        switch (f) {
        case MXF_Dest: return Fswzadd_Dest(o);
        case MXF_SrcA: return Fswzadd_SrcA(o);
        case MXF_SrcB: return Fswzadd_SrcB(o);
        case MXF_Pred: return Fswzadd_Pred(o);
        case MXF_PredInv: return Fswzadd_PredInv(o);
        case MXF_WriteCC: return Fswzadd_WriteCC(o);
        case MXF_Ftz: return Fswzadd_Ftz(o);
        case MXF_RoundMode: return Fswzadd_RoundMode(o);
        case MXF_Ndv: return Fswzadd_Ndv(o);
        case MXF_PnWord: return Fswzadd_PnWord(o);
        }
        return MX_NO;
    case MXOP_Getcrsptr:
        switch (f) {
        case MXF_Dest: return Getcrsptr_Dest(o);
        }
        return MX_NO;
    case MXOP_Getlmembase:
        switch (f) {
        case MXF_Dest: return Getlmembase_Dest(o);
        }
        return MX_NO;
    case MXOP_Hadd232i:
        switch (f) {
        case MXF_Dest: return Hadd232i_Dest(o);
        case MXF_SrcA: return Hadd232i_SrcA(o);
        case MXF_Imm: return Hadd232i_Imm(o);
        case MXF_ASwizzle: return Hadd232i_ASwizzle(o);
        case MXF_Pred: return Hadd232i_Pred(o);
        case MXF_PredInv: return Hadd232i_PredInv(o);
        case MXF_NegA: return Hadd232i_NegA(o);
        case MXF_Sat: return Hadd232i_Sat(o);
        case MXF_Ftz: return Hadd232i_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hadd2C:
        switch (f) {
        case MXF_Dest: return Hadd2C_Dest(o);
        case MXF_SrcA: return Hadd2C_SrcA(o);
        case MXF_CbufSlot: return Hadd2C_CbufSlot(o);
        case MXF_CbufOffset: return Hadd2C_CbufOffset(o);
        case MXF_OFmt: return Hadd2C_OFmt(o);
        case MXF_ASwizzle: return Hadd2C_ASwizzle(o);
        case MXF_Pred: return Hadd2C_Pred(o);
        case MXF_PredInv: return Hadd2C_PredInv(o);
        case MXF_NegA: return Hadd2C_NegA(o);
        case MXF_NegB: return Hadd2C_NegB(o);
        case MXF_AbsA: return Hadd2C_AbsA(o);
        case MXF_AbsB: return Hadd2C_AbsB(o);
        case MXF_Sat: return Hadd2C_Sat(o);
        case MXF_Ftz: return Hadd2C_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hadd2I:
        switch (f) {
        case MXF_Dest: return Hadd2I_Dest(o);
        case MXF_SrcA: return Hadd2I_SrcA(o);
        case MXF_BimmH0: return Hadd2I_BimmH0(o);
        case MXF_BimmH1: return Hadd2I_BimmH1(o);
        case MXF_OFmt: return Hadd2I_OFmt(o);
        case MXF_ASwizzle: return Hadd2I_ASwizzle(o);
        case MXF_Pred: return Hadd2I_Pred(o);
        case MXF_PredInv: return Hadd2I_PredInv(o);
        case MXF_NegA: return Hadd2I_NegA(o);
        case MXF_AbsA: return Hadd2I_AbsA(o);
        case MXF_Sat: return Hadd2I_Sat(o);
        case MXF_Ftz: return Hadd2I_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hadd2R:
        switch (f) {
        case MXF_Dest: return Hadd2R_Dest(o);
        case MXF_SrcA: return Hadd2R_SrcA(o);
        case MXF_SrcB: return Hadd2R_SrcB(o);
        case MXF_OFmt: return Hadd2R_OFmt(o);
        case MXF_ASwizzle: return Hadd2R_ASwizzle(o);
        case MXF_BSwizzle: return Hadd2R_BSwizzle(o);
        case MXF_Pred: return Hadd2R_Pred(o);
        case MXF_PredInv: return Hadd2R_PredInv(o);
        case MXF_NegA: return Hadd2R_NegA(o);
        case MXF_NegB: return Hadd2R_NegB(o);
        case MXF_AbsA: return Hadd2R_AbsA(o);
        case MXF_AbsB: return Hadd2R_AbsB(o);
        case MXF_Sat: return Hadd2R_Sat(o);
        case MXF_Ftz: return Hadd2R_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hfma232i:
        switch (f) {
        case MXF_Dest: return Hfma232i_Dest(o);
        case MXF_SrcA: return Hfma232i_SrcA(o);
        case MXF_Imm: return Hfma232i_Imm(o);
        case MXF_ASwizzle: return Hfma232i_ASwizzle(o);
        case MXF_Pred: return Hfma232i_Pred(o);
        case MXF_PredInv: return Hfma232i_PredInv(o);
        case MXF_NegC: return Hfma232i_NegC(o);
        case MXF_Fmz: return Hfma232i_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hfma2C:
        switch (f) {
        case MXF_Dest: return Hfma2C_Dest(o);
        case MXF_SrcA: return Hfma2C_SrcA(o);
        case MXF_CbufSlot: return Hfma2C_CbufSlot(o);
        case MXF_CbufOffset: return Hfma2C_CbufOffset(o);
        case MXF_SrcC: return Hfma2C_SrcC(o);
        case MXF_OFmt: return Hfma2C_OFmt(o);
        case MXF_ASwizzle: return Hfma2C_ASwizzle(o);
        case MXF_CSwizzle: return Hfma2C_CSwizzle(o);
        case MXF_Pred: return Hfma2C_Pred(o);
        case MXF_PredInv: return Hfma2C_PredInv(o);
        case MXF_NegA: return Hfma2C_NegA(o);
        case MXF_NegC: return Hfma2C_NegC(o);
        case MXF_Sat: return Hfma2C_Sat(o);
        case MXF_Fmz: return Hfma2C_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hfma2I:
        switch (f) {
        case MXF_Dest: return Hfma2I_Dest(o);
        case MXF_SrcA: return Hfma2I_SrcA(o);
        case MXF_BimmH0: return Hfma2I_BimmH0(o);
        case MXF_BimmH1: return Hfma2I_BimmH1(o);
        case MXF_SrcC: return Hfma2I_SrcC(o);
        case MXF_OFmt: return Hfma2I_OFmt(o);
        case MXF_ASwizzle: return Hfma2I_ASwizzle(o);
        case MXF_CSwizzle: return Hfma2I_CSwizzle(o);
        case MXF_Pred: return Hfma2I_Pred(o);
        case MXF_PredInv: return Hfma2I_PredInv(o);
        case MXF_NegC: return Hfma2I_NegC(o);
        case MXF_Sat: return Hfma2I_Sat(o);
        case MXF_Fmz: return Hfma2I_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hfma2R:
        switch (f) {
        case MXF_Dest: return Hfma2R_Dest(o);
        case MXF_SrcA: return Hfma2R_SrcA(o);
        case MXF_SrcB: return Hfma2R_SrcB(o);
        case MXF_SrcC: return Hfma2R_SrcC(o);
        case MXF_OFmt: return Hfma2R_OFmt(o);
        case MXF_ASwizzle: return Hfma2R_ASwizzle(o);
        case MXF_BSwizzle: return Hfma2R_BSwizzle(o);
        case MXF_CSwizzle: return Hfma2R_CSwizzle(o);
        case MXF_Pred: return Hfma2R_Pred(o);
        case MXF_PredInv: return Hfma2R_PredInv(o);
        case MXF_NegA: return Hfma2R_NegA(o);
        case MXF_NegC: return Hfma2R_NegC(o);
        case MXF_Sat: return Hfma2R_Sat(o);
        case MXF_Fmz: return Hfma2R_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hfma2Rc:
        switch (f) {
        case MXF_Dest: return Hfma2Rc_Dest(o);
        case MXF_SrcA: return Hfma2Rc_SrcA(o);
        case MXF_CbufSlot: return Hfma2Rc_CbufSlot(o);
        case MXF_CbufOffset: return Hfma2Rc_CbufOffset(o);
        case MXF_SrcC: return Hfma2Rc_SrcC(o);
        case MXF_OFmt: return Hfma2Rc_OFmt(o);
        case MXF_ASwizzle: return Hfma2Rc_ASwizzle(o);
        case MXF_CSwizzle: return Hfma2Rc_CSwizzle(o);
        case MXF_Pred: return Hfma2Rc_Pred(o);
        case MXF_PredInv: return Hfma2Rc_PredInv(o);
        case MXF_NegA: return Hfma2Rc_NegA(o);
        case MXF_NegC: return Hfma2Rc_NegC(o);
        case MXF_Sat: return Hfma2Rc_Sat(o);
        case MXF_Fmz: return Hfma2Rc_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hmul232i:
        switch (f) {
        case MXF_Dest: return Hmul232i_Dest(o);
        case MXF_SrcA: return Hmul232i_SrcA(o);
        case MXF_Imm32: return Hmul232i_Imm32(o);
        case MXF_ASwizzle: return Hmul232i_ASwizzle(o);
        case MXF_Pred: return Hmul232i_Pred(o);
        case MXF_PredInv: return Hmul232i_PredInv(o);
        case MXF_Sat: return Hmul232i_Sat(o);
        case MXF_Fmz: return Hmul232i_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hmul2C:
        switch (f) {
        case MXF_Dest: return Hmul2C_Dest(o);
        case MXF_SrcA: return Hmul2C_SrcA(o);
        case MXF_CbufSlot: return Hmul2C_CbufSlot(o);
        case MXF_CbufOffset: return Hmul2C_CbufOffset(o);
        case MXF_OFmt: return Hmul2C_OFmt(o);
        case MXF_ASwizzle: return Hmul2C_ASwizzle(o);
        case MXF_Pred: return Hmul2C_Pred(o);
        case MXF_PredInv: return Hmul2C_PredInv(o);
        case MXF_NegA: return Hmul2C_NegA(o);
        case MXF_AbsA: return Hmul2C_AbsA(o);
        case MXF_AbsB: return Hmul2C_AbsB(o);
        case MXF_Sat: return Hmul2C_Sat(o);
        case MXF_Fmz: return Hmul2C_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hmul2I:
        switch (f) {
        case MXF_Dest: return Hmul2I_Dest(o);
        case MXF_SrcA: return Hmul2I_SrcA(o);
        case MXF_BimmH0: return Hmul2I_BimmH0(o);
        case MXF_BimmH1: return Hmul2I_BimmH1(o);
        case MXF_OFmt: return Hmul2I_OFmt(o);
        case MXF_ASwizzle: return Hmul2I_ASwizzle(o);
        case MXF_Pred: return Hmul2I_Pred(o);
        case MXF_PredInv: return Hmul2I_PredInv(o);
        case MXF_NegA: return Hmul2I_NegA(o);
        case MXF_AbsA: return Hmul2I_AbsA(o);
        case MXF_Sat: return Hmul2I_Sat(o);
        case MXF_Fmz: return Hmul2I_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hmul2R:
        switch (f) {
        case MXF_Dest: return Hmul2R_Dest(o);
        case MXF_SrcA: return Hmul2R_SrcA(o);
        case MXF_SrcB: return Hmul2R_SrcB(o);
        case MXF_OFmt: return Hmul2R_OFmt(o);
        case MXF_ASwizzle: return Hmul2R_ASwizzle(o);
        case MXF_BSwizzle: return Hmul2R_BSwizzle(o);
        case MXF_Pred: return Hmul2R_Pred(o);
        case MXF_PredInv: return Hmul2R_PredInv(o);
        case MXF_NegA: return Hmul2R_NegA(o);
        case MXF_AbsA: return Hmul2R_AbsA(o);
        case MXF_AbsB: return Hmul2R_AbsB(o);
        case MXF_Sat: return Hmul2R_Sat(o);
        case MXF_Fmz: return Hmul2R_Fmz(o);
        }
        return MX_NO;
    case MXOP_Hset2C:
        switch (f) {
        case MXF_Dest: return Hset2C_Dest(o);
        case MXF_SrcA: return Hset2C_SrcA(o);
        case MXF_CbufSlot: return Hset2C_CbufSlot(o);
        case MXF_CbufOffset: return Hset2C_CbufOffset(o);
        case MXF_ASwizzle: return Hset2C_ASwizzle(o);
        case MXF_Pred: return Hset2C_Pred(o);
        case MXF_PredInv: return Hset2C_PredInv(o);
        case MXF_NegA: return Hset2C_NegA(o);
        case MXF_AbsA: return Hset2C_AbsA(o);
        case MXF_NegB: return Hset2C_NegB(o);
        case MXF_Bval: return Hset2C_Bval(o);
        case MXF_Cmp: return Hset2C_Cmp(o);
        case MXF_Bop: return Hset2C_Bop(o);
        case MXF_SrcPred: return Hset2C_SrcPred(o);
        case MXF_SrcPredInv: return Hset2C_SrcPredInv(o);
        case MXF_Ftz: return Hset2C_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hset2I:
        switch (f) {
        case MXF_Dest: return Hset2I_Dest(o);
        case MXF_SrcA: return Hset2I_SrcA(o);
        case MXF_BimmH0: return Hset2I_BimmH0(o);
        case MXF_BimmH1: return Hset2I_BimmH1(o);
        case MXF_ASwizzle: return Hset2I_ASwizzle(o);
        case MXF_Pred: return Hset2I_Pred(o);
        case MXF_PredInv: return Hset2I_PredInv(o);
        case MXF_NegA: return Hset2I_NegA(o);
        case MXF_AbsA: return Hset2I_AbsA(o);
        case MXF_Bval: return Hset2I_Bval(o);
        case MXF_Cmp: return Hset2I_Cmp(o);
        case MXF_Bop: return Hset2I_Bop(o);
        case MXF_SrcPred: return Hset2I_SrcPred(o);
        case MXF_SrcPredInv: return Hset2I_SrcPredInv(o);
        case MXF_Ftz: return Hset2I_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hset2R:
        switch (f) {
        case MXF_Dest: return Hset2R_Dest(o);
        case MXF_SrcA: return Hset2R_SrcA(o);
        case MXF_SrcB: return Hset2R_SrcB(o);
        case MXF_ASwizzle: return Hset2R_ASwizzle(o);
        case MXF_BSwizzle: return Hset2R_BSwizzle(o);
        case MXF_Pred: return Hset2R_Pred(o);
        case MXF_PredInv: return Hset2R_PredInv(o);
        case MXF_NegA: return Hset2R_NegA(o);
        case MXF_AbsA: return Hset2R_AbsA(o);
        case MXF_NegB: return Hset2R_NegB(o);
        case MXF_AbsB: return Hset2R_AbsB(o);
        case MXF_Bval: return Hset2R_Bval(o);
        case MXF_Cmp: return Hset2R_Cmp(o);
        case MXF_Bop: return Hset2R_Bop(o);
        case MXF_SrcPred: return Hset2R_SrcPred(o);
        case MXF_SrcPredInv: return Hset2R_SrcPredInv(o);
        case MXF_Ftz: return Hset2R_Ftz(o);
        }
        return MX_NO;
    case MXOP_Hsetp2C:
        switch (f) {
        case MXF_DestPred: return Hsetp2C_DestPred(o);
        case MXF_DestPredInv: return Hsetp2C_DestPredInv(o);
        case MXF_SrcA: return Hsetp2C_SrcA(o);
        case MXF_CbufSlot: return Hsetp2C_CbufSlot(o);
        case MXF_CbufOffset: return Hsetp2C_CbufOffset(o);
        case MXF_Pred: return Hsetp2C_Pred(o);
        case MXF_PredInv: return Hsetp2C_PredInv(o);
        case MXF_NegA: return Hsetp2C_NegA(o);
        case MXF_NegB: return Hsetp2C_NegB(o);
        case MXF_AbsA: return Hsetp2C_AbsA(o);
        case MXF_AbsB: return Hsetp2C_AbsB(o);
        case MXF_FComp: return Hsetp2C_FComp(o);
        case MXF_SrcPred: return Hsetp2C_SrcPred(o);
        case MXF_SrcPredInv: return Hsetp2C_SrcPredInv(o);
        case MXF_Bop: return Hsetp2C_Bop(o);
        case MXF_Ftz: return Hsetp2C_Ftz(o);
        case MXF_HAnd: return Hsetp2C_HAnd(o);
        case MXF_ASwizzle: return Hsetp2C_ASwizzle(o);
        }
        return MX_NO;
    case MXOP_Hsetp2I:
        switch (f) {
        case MXF_DestPred: return Hsetp2I_DestPred(o);
        case MXF_DestPredInv: return Hsetp2I_DestPredInv(o);
        case MXF_SrcA: return Hsetp2I_SrcA(o);
        case MXF_Pred: return Hsetp2I_Pred(o);
        case MXF_PredInv: return Hsetp2I_PredInv(o);
        case MXF_BimmH0: return Hsetp2I_BimmH0(o);
        case MXF_BimmH1: return Hsetp2I_BimmH1(o);
        case MXF_NegA: return Hsetp2I_NegA(o);
        case MXF_AbsA: return Hsetp2I_AbsA(o);
        case MXF_FComp: return Hsetp2I_FComp(o);
        case MXF_SrcPred: return Hsetp2I_SrcPred(o);
        case MXF_SrcPredInv: return Hsetp2I_SrcPredInv(o);
        case MXF_Bop: return Hsetp2I_Bop(o);
        case MXF_Ftz: return Hsetp2I_Ftz(o);
        case MXF_HAnd: return Hsetp2I_HAnd(o);
        case MXF_ASwizzle: return Hsetp2I_ASwizzle(o);
        }
        return MX_NO;
    case MXOP_Hsetp2R:
        switch (f) {
        case MXF_DestPred: return Hsetp2R_DestPred(o);
        case MXF_DestPredInv: return Hsetp2R_DestPredInv(o);
        case MXF_SrcA: return Hsetp2R_SrcA(o);
        case MXF_SrcB: return Hsetp2R_SrcB(o);
        case MXF_Pred: return Hsetp2R_Pred(o);
        case MXF_PredInv: return Hsetp2R_PredInv(o);
        case MXF_NegA: return Hsetp2R_NegA(o);
        case MXF_NegB: return Hsetp2R_NegB(o);
        case MXF_AbsA: return Hsetp2R_AbsA(o);
        case MXF_AbsB: return Hsetp2R_AbsB(o);
        case MXF_FComp2: return Hsetp2R_FComp2(o);
        case MXF_SrcPred: return Hsetp2R_SrcPred(o);
        case MXF_SrcPredInv: return Hsetp2R_SrcPredInv(o);
        case MXF_Bop: return Hsetp2R_Bop(o);
        case MXF_Ftz: return Hsetp2R_Ftz(o);
        case MXF_HAnd: return Hsetp2R_HAnd(o);
        case MXF_ASwizzle: return Hsetp2R_ASwizzle(o);
        case MXF_BSwizzle: return Hsetp2R_BSwizzle(o);
        }
        return MX_NO;
    case MXOP_I2fC:
        switch (f) {
        case MXF_Dest: return I2fC_Dest(o);
        case MXF_CbufSlot: return I2fC_CbufSlot(o);
        case MXF_CbufOffset: return I2fC_CbufOffset(o);
        case MXF_Pred: return I2fC_Pred(o);
        case MXF_PredInv: return I2fC_PredInv(o);
        case MXF_WriteCC: return I2fC_WriteCC(o);
        case MXF_AbsB: return I2fC_AbsB(o);
        case MXF_NegB: return I2fC_NegB(o);
        case MXF_ByteSel: return I2fC_ByteSel(o);
        case MXF_RoundMode: return I2fC_RoundMode(o);
        case MXF_ISrcFmt: return I2fC_ISrcFmt(o);
        case MXF_DstFmt: return I2fC_DstFmt(o);
        }
        return MX_NO;
    case MXOP_I2fI:
        switch (f) {
        case MXF_Dest: return I2fI_Dest(o);
        case MXF_Imm20: return I2fI_Imm20(o);
        case MXF_Pred: return I2fI_Pred(o);
        case MXF_PredInv: return I2fI_PredInv(o);
        case MXF_WriteCC: return I2fI_WriteCC(o);
        case MXF_AbsB: return I2fI_AbsB(o);
        case MXF_NegB: return I2fI_NegB(o);
        case MXF_ByteSel: return I2fI_ByteSel(o);
        case MXF_RoundMode: return I2fI_RoundMode(o);
        case MXF_ISrcFmt: return I2fI_ISrcFmt(o);
        case MXF_DstFmt: return I2fI_DstFmt(o);
        }
        return MX_NO;
    case MXOP_I2fR:
        switch (f) {
        case MXF_Dest: return I2fR_Dest(o);
        case MXF_SrcB: return I2fR_SrcB(o);
        case MXF_Pred: return I2fR_Pred(o);
        case MXF_PredInv: return I2fR_PredInv(o);
        case MXF_WriteCC: return I2fR_WriteCC(o);
        case MXF_AbsB: return I2fR_AbsB(o);
        case MXF_NegB: return I2fR_NegB(o);
        case MXF_ByteSel: return I2fR_ByteSel(o);
        case MXF_RoundMode: return I2fR_RoundMode(o);
        case MXF_ISrcFmt: return I2fR_ISrcFmt(o);
        case MXF_DstFmt: return I2fR_DstFmt(o);
        }
        return MX_NO;
    case MXOP_I2iC:
        switch (f) {
        case MXF_Dest: return I2iC_Dest(o);
        case MXF_CbufSlot: return I2iC_CbufSlot(o);
        case MXF_CbufOffset: return I2iC_CbufOffset(o);
        case MXF_Pred: return I2iC_Pred(o);
        case MXF_PredInv: return I2iC_PredInv(o);
        case MXF_WriteCC: return I2iC_WriteCC(o);
        case MXF_Sat: return I2iC_Sat(o);
        case MXF_AbsB: return I2iC_AbsB(o);
        case MXF_NegB: return I2iC_NegB(o);
        case MXF_ByteSel: return I2iC_ByteSel(o);
        case MXF_IDstFmt: return I2iC_IDstFmt(o);
        case MXF_ISrcFmt: return I2iC_ISrcFmt(o);
        }
        return MX_NO;
    case MXOP_I2iI:
        switch (f) {
        case MXF_Dest: return I2iI_Dest(o);
        case MXF_Imm20: return I2iI_Imm20(o);
        case MXF_Pred: return I2iI_Pred(o);
        case MXF_PredInv: return I2iI_PredInv(o);
        case MXF_WriteCC: return I2iI_WriteCC(o);
        case MXF_Sat: return I2iI_Sat(o);
        case MXF_AbsB: return I2iI_AbsB(o);
        case MXF_NegB: return I2iI_NegB(o);
        case MXF_ByteSel: return I2iI_ByteSel(o);
        case MXF_IDstFmt: return I2iI_IDstFmt(o);
        case MXF_ISrcFmt: return I2iI_ISrcFmt(o);
        }
        return MX_NO;
    case MXOP_I2iR:
        switch (f) {
        case MXF_Dest: return I2iR_Dest(o);
        case MXF_SrcB: return I2iR_SrcB(o);
        case MXF_Pred: return I2iR_Pred(o);
        case MXF_PredInv: return I2iR_PredInv(o);
        case MXF_WriteCC: return I2iR_WriteCC(o);
        case MXF_Sat: return I2iR_Sat(o);
        case MXF_AbsB: return I2iR_AbsB(o);
        case MXF_NegB: return I2iR_NegB(o);
        case MXF_ByteSel: return I2iR_ByteSel(o);
        case MXF_IDstFmt: return I2iR_IDstFmt(o);
        case MXF_ISrcFmt: return I2iR_ISrcFmt(o);
        }
        return MX_NO;
    case MXOP_Iadd32i:
        switch (f) {
        case MXF_Dest: return Iadd32i_Dest(o);
        case MXF_SrcA: return Iadd32i_SrcA(o);
        case MXF_Pred: return Iadd32i_Pred(o);
        case MXF_PredInv: return Iadd32i_PredInv(o);
        case MXF_Imm32: return Iadd32i_Imm32(o);
        case MXF_AvgMode: return Iadd32i_AvgMode(o);
        case MXF_Sat: return Iadd32i_Sat(o);
        case MXF_WriteCC: return Iadd32i_WriteCC(o);
        case MXF_X: return Iadd32i_X(o);
        }
        return MX_NO;
    case MXOP_Iadd3C:
        switch (f) {
        case MXF_Dest: return Iadd3C_Dest(o);
        case MXF_SrcA: return Iadd3C_SrcA(o);
        case MXF_CbufSlot: return Iadd3C_CbufSlot(o);
        case MXF_CbufOffset: return Iadd3C_CbufOffset(o);
        case MXF_SrcC: return Iadd3C_SrcC(o);
        case MXF_Pred: return Iadd3C_Pred(o);
        case MXF_PredInv: return Iadd3C_PredInv(o);
        case MXF_WriteCC: return Iadd3C_WriteCC(o);
        case MXF_NegA: return Iadd3C_NegA(o);
        case MXF_NegB: return Iadd3C_NegB(o);
        case MXF_NegC: return Iadd3C_NegC(o);
        case MXF_X: return Iadd3C_X(o);
        }
        return MX_NO;
    case MXOP_Iadd3I:
        switch (f) {
        case MXF_Dest: return Iadd3I_Dest(o);
        case MXF_SrcA: return Iadd3I_SrcA(o);
        case MXF_Imm20: return Iadd3I_Imm20(o);
        case MXF_SrcC: return Iadd3I_SrcC(o);
        case MXF_Pred: return Iadd3I_Pred(o);
        case MXF_PredInv: return Iadd3I_PredInv(o);
        case MXF_WriteCC: return Iadd3I_WriteCC(o);
        case MXF_NegA: return Iadd3I_NegA(o);
        case MXF_NegB: return Iadd3I_NegB(o);
        case MXF_NegC: return Iadd3I_NegC(o);
        case MXF_X: return Iadd3I_X(o);
        }
        return MX_NO;
    case MXOP_Iadd3R:
        switch (f) {
        case MXF_Dest: return Iadd3R_Dest(o);
        case MXF_SrcA: return Iadd3R_SrcA(o);
        case MXF_SrcB: return Iadd3R_SrcB(o);
        case MXF_SrcC: return Iadd3R_SrcC(o);
        case MXF_Pred: return Iadd3R_Pred(o);
        case MXF_PredInv: return Iadd3R_PredInv(o);
        case MXF_WriteCC: return Iadd3R_WriteCC(o);
        case MXF_NegA: return Iadd3R_NegA(o);
        case MXF_NegB: return Iadd3R_NegB(o);
        case MXF_NegC: return Iadd3R_NegC(o);
        case MXF_X: return Iadd3R_X(o);
        case MXF_Lrs: return Iadd3R_Lrs(o);
        case MXF_Apart: return Iadd3R_Apart(o);
        case MXF_Bpart: return Iadd3R_Bpart(o);
        case MXF_Cpart: return Iadd3R_Cpart(o);
        }
        return MX_NO;
    case MXOP_IaddC:
        switch (f) {
        case MXF_Dest: return IaddC_Dest(o);
        case MXF_SrcA: return IaddC_SrcA(o);
        case MXF_CbufSlot: return IaddC_CbufSlot(o);
        case MXF_CbufOffset: return IaddC_CbufOffset(o);
        case MXF_Pred: return IaddC_Pred(o);
        case MXF_PredInv: return IaddC_PredInv(o);
        case MXF_WriteCC: return IaddC_WriteCC(o);
        case MXF_Sat: return IaddC_Sat(o);
        case MXF_AvgMode: return IaddC_AvgMode(o);
        case MXF_X: return IaddC_X(o);
        }
        return MX_NO;
    case MXOP_IaddI:
        switch (f) {
        case MXF_Dest: return IaddI_Dest(o);
        case MXF_SrcA: return IaddI_SrcA(o);
        case MXF_Imm20: return IaddI_Imm20(o);
        case MXF_Pred: return IaddI_Pred(o);
        case MXF_PredInv: return IaddI_PredInv(o);
        case MXF_WriteCC: return IaddI_WriteCC(o);
        case MXF_Sat: return IaddI_Sat(o);
        case MXF_AvgMode: return IaddI_AvgMode(o);
        case MXF_X: return IaddI_X(o);
        }
        return MX_NO;
    case MXOP_IaddR:
        switch (f) {
        case MXF_Dest: return IaddR_Dest(o);
        case MXF_SrcA: return IaddR_SrcA(o);
        case MXF_SrcB: return IaddR_SrcB(o);
        case MXF_Pred: return IaddR_Pred(o);
        case MXF_PredInv: return IaddR_PredInv(o);
        case MXF_WriteCC: return IaddR_WriteCC(o);
        case MXF_Sat: return IaddR_Sat(o);
        case MXF_AvgMode: return IaddR_AvgMode(o);
        case MXF_X: return IaddR_X(o);
        }
        return MX_NO;
    case MXOP_IcmpC:
        switch (f) {
        case MXF_Dest: return IcmpC_Dest(o);
        case MXF_SrcA: return IcmpC_SrcA(o);
        case MXF_CbufSlot: return IcmpC_CbufSlot(o);
        case MXF_CbufOffset: return IcmpC_CbufOffset(o);
        case MXF_SrcC: return IcmpC_SrcC(o);
        case MXF_Pred: return IcmpC_Pred(o);
        case MXF_PredInv: return IcmpC_PredInv(o);
        case MXF_IComp: return IcmpC_IComp(o);
        case MXF_Signed: return IcmpC_Signed(o);
        }
        return MX_NO;
    case MXOP_IcmpI:
        switch (f) {
        case MXF_Dest: return IcmpI_Dest(o);
        case MXF_SrcA: return IcmpI_SrcA(o);
        case MXF_Imm20: return IcmpI_Imm20(o);
        case MXF_SrcC: return IcmpI_SrcC(o);
        case MXF_Pred: return IcmpI_Pred(o);
        case MXF_PredInv: return IcmpI_PredInv(o);
        case MXF_IComp: return IcmpI_IComp(o);
        case MXF_Signed: return IcmpI_Signed(o);
        }
        return MX_NO;
    case MXOP_IcmpR:
        switch (f) {
        case MXF_Dest: return IcmpR_Dest(o);
        case MXF_SrcA: return IcmpR_SrcA(o);
        case MXF_SrcB: return IcmpR_SrcB(o);
        case MXF_SrcC: return IcmpR_SrcC(o);
        case MXF_Pred: return IcmpR_Pred(o);
        case MXF_PredInv: return IcmpR_PredInv(o);
        case MXF_IComp: return IcmpR_IComp(o);
        case MXF_Signed: return IcmpR_Signed(o);
        }
        return MX_NO;
    case MXOP_IcmpRc:
        switch (f) {
        case MXF_Dest: return IcmpRc_Dest(o);
        case MXF_SrcA: return IcmpRc_SrcA(o);
        case MXF_SrcC: return IcmpRc_SrcC(o);
        case MXF_CbufSlot: return IcmpRc_CbufSlot(o);
        case MXF_CbufOffset: return IcmpRc_CbufOffset(o);
        case MXF_Pred: return IcmpRc_Pred(o);
        case MXF_PredInv: return IcmpRc_PredInv(o);
        case MXF_IComp: return IcmpRc_IComp(o);
        case MXF_Signed: return IcmpRc_Signed(o);
        }
        return MX_NO;
    case MXOP_Ide:
        switch (f) {
        case MXF_Imm16: return Ide_Imm16(o);
        case MXF_Di: return Ide_Di(o);
        }
        return MX_NO;
    case MXOP_IdpC:
        switch (f) {
        case MXF_Dest: return IdpC_Dest(o);
        case MXF_SrcA: return IdpC_SrcA(o);
        case MXF_CbufSlot: return IdpC_CbufSlot(o);
        case MXF_CbufOffset: return IdpC_CbufOffset(o);
        case MXF_SrcC: return IdpC_SrcC(o);
        case MXF_Pred: return IdpC_Pred(o);
        case MXF_PredInv: return IdpC_PredInv(o);
        case MXF_IsHi: return IdpC_IsHi(o);
        case MXF_SrcASign: return IdpC_SrcASign(o);
        case MXF_IsDp: return IdpC_IsDp(o);
        case MXF_SrcBSign: return IdpC_SrcBSign(o);
        }
        return MX_NO;
    case MXOP_IdpR:
        switch (f) {
        case MXF_Dest: return IdpR_Dest(o);
        case MXF_SrcA: return IdpR_SrcA(o);
        case MXF_SrcB: return IdpR_SrcB(o);
        case MXF_SrcC: return IdpR_SrcC(o);
        case MXF_Pred: return IdpR_Pred(o);
        case MXF_PredInv: return IdpR_PredInv(o);
        case MXF_IsHi: return IdpR_IsHi(o);
        case MXF_SrcASign: return IdpR_SrcASign(o);
        case MXF_IsDp: return IdpR_IsDp(o);
        case MXF_SrcBSign: return IdpR_SrcBSign(o);
        }
        return MX_NO;
    case MXOP_Imad32i:
        switch (f) {
        case MXF_Dest: return Imad32i_Dest(o);
        case MXF_SrcA: return Imad32i_SrcA(o);
        case MXF_Pred: return Imad32i_Pred(o);
        case MXF_PredInv: return Imad32i_PredInv(o);
        case MXF_Imm32: return Imad32i_Imm32(o);
        case MXF_BSigned: return Imad32i_BSigned(o);
        case MXF_AvgMode: return Imad32i_AvgMode(o);
        case MXF_ASigned: return Imad32i_ASigned(o);
        case MXF_WriteCC: return Imad32i_WriteCC(o);
        case MXF_Hilo: return Imad32i_Hilo(o);
        }
        return MX_NO;
    case MXOP_ImadC:
        switch (f) {
        case MXF_Dest: return ImadC_Dest(o);
        case MXF_SrcA: return ImadC_SrcA(o);
        case MXF_CbufSlot: return ImadC_CbufSlot(o);
        case MXF_CbufOffset: return ImadC_CbufOffset(o);
        case MXF_SrcC: return ImadC_SrcC(o);
        case MXF_Pred: return ImadC_Pred(o);
        case MXF_PredInv: return ImadC_PredInv(o);
        case MXF_WriteCC: return ImadC_WriteCC(o);
        case MXF_Hilo: return ImadC_Hilo(o);
        case MXF_BSigned: return ImadC_BSigned(o);
        case MXF_AvgMode: return ImadC_AvgMode(o);
        case MXF_Sat: return ImadC_Sat(o);
        case MXF_X: return ImadC_X(o);
        case MXF_ASigned: return ImadC_ASigned(o);
        }
        return MX_NO;
    case MXOP_ImadI:
        switch (f) {
        case MXF_Dest: return ImadI_Dest(o);
        case MXF_SrcA: return ImadI_SrcA(o);
        case MXF_Imm20: return ImadI_Imm20(o);
        case MXF_SrcC: return ImadI_SrcC(o);
        case MXF_Pred: return ImadI_Pred(o);
        case MXF_PredInv: return ImadI_PredInv(o);
        case MXF_WriteCC: return ImadI_WriteCC(o);
        case MXF_Hilo: return ImadI_Hilo(o);
        case MXF_BSigned: return ImadI_BSigned(o);
        case MXF_AvgMode: return ImadI_AvgMode(o);
        case MXF_Sat: return ImadI_Sat(o);
        case MXF_X: return ImadI_X(o);
        case MXF_ASigned: return ImadI_ASigned(o);
        }
        return MX_NO;
    case MXOP_ImadR:
        switch (f) {
        case MXF_Dest: return ImadR_Dest(o);
        case MXF_SrcA: return ImadR_SrcA(o);
        case MXF_SrcB: return ImadR_SrcB(o);
        case MXF_SrcC: return ImadR_SrcC(o);
        case MXF_Pred: return ImadR_Pred(o);
        case MXF_PredInv: return ImadR_PredInv(o);
        case MXF_WriteCC: return ImadR_WriteCC(o);
        case MXF_Hilo: return ImadR_Hilo(o);
        case MXF_BSigned: return ImadR_BSigned(o);
        case MXF_AvgMode: return ImadR_AvgMode(o);
        case MXF_Sat: return ImadR_Sat(o);
        case MXF_X: return ImadR_X(o);
        case MXF_ASigned: return ImadR_ASigned(o);
        }
        return MX_NO;
    case MXOP_ImadRc:
        switch (f) {
        case MXF_Dest: return ImadRc_Dest(o);
        case MXF_SrcA: return ImadRc_SrcA(o);
        case MXF_SrcC: return ImadRc_SrcC(o);
        case MXF_CbufSlot: return ImadRc_CbufSlot(o);
        case MXF_CbufOffset: return ImadRc_CbufOffset(o);
        case MXF_Pred: return ImadRc_Pred(o);
        case MXF_PredInv: return ImadRc_PredInv(o);
        case MXF_WriteCC: return ImadRc_WriteCC(o);
        case MXF_Hilo: return ImadRc_Hilo(o);
        case MXF_BSigned: return ImadRc_BSigned(o);
        case MXF_AvgMode: return ImadRc_AvgMode(o);
        case MXF_Sat: return ImadRc_Sat(o);
        case MXF_X: return ImadRc_X(o);
        case MXF_ASigned: return ImadRc_ASigned(o);
        }
        return MX_NO;
    case MXOP_ImadspC:
        switch (f) {
        case MXF_Dest: return ImadspC_Dest(o);
        case MXF_SrcA: return ImadspC_SrcA(o);
        case MXF_CbufSlot: return ImadspC_CbufSlot(o);
        case MXF_CbufOffset: return ImadspC_CbufOffset(o);
        case MXF_SrcC: return ImadspC_SrcC(o);
        case MXF_Pred: return ImadspC_Pred(o);
        case MXF_PredInv: return ImadspC_PredInv(o);
        case MXF_ASelect: return ImadspC_ASelect(o);
        case MXF_BSelect: return ImadspC_BSelect(o);
        case MXF_CSelect: return ImadspC_CSelect(o);
        }
        return MX_NO;
    case MXOP_ImadspI:
        switch (f) {
        case MXF_Dest: return ImadspI_Dest(o);
        case MXF_SrcA: return ImadspI_SrcA(o);
        case MXF_Imm20: return ImadspI_Imm20(o);
        case MXF_SrcC: return ImadspI_SrcC(o);
        case MXF_Pred: return ImadspI_Pred(o);
        case MXF_PredInv: return ImadspI_PredInv(o);
        case MXF_ASelect: return ImadspI_ASelect(o);
        case MXF_BSelect: return ImadspI_BSelect(o);
        case MXF_CSelect: return ImadspI_CSelect(o);
        }
        return MX_NO;
    case MXOP_ImadspR:
        switch (f) {
        case MXF_Dest: return ImadspR_Dest(o);
        case MXF_SrcA: return ImadspR_SrcA(o);
        case MXF_SrcB: return ImadspR_SrcB(o);
        case MXF_SrcC: return ImadspR_SrcC(o);
        case MXF_Pred: return ImadspR_Pred(o);
        case MXF_PredInv: return ImadspR_PredInv(o);
        case MXF_ASelect: return ImadspR_ASelect(o);
        case MXF_BSelect: return ImadspR_BSelect(o);
        case MXF_CSelect: return ImadspR_CSelect(o);
        }
        return MX_NO;
    case MXOP_ImadspRc:
        switch (f) {
        case MXF_Dest: return ImadspRc_Dest(o);
        case MXF_SrcA: return ImadspRc_SrcA(o);
        case MXF_SrcC: return ImadspRc_SrcC(o);
        case MXF_CbufSlot: return ImadspRc_CbufSlot(o);
        case MXF_CbufOffset: return ImadspRc_CbufOffset(o);
        case MXF_Pred: return ImadspRc_Pred(o);
        case MXF_PredInv: return ImadspRc_PredInv(o);
        case MXF_ASelect: return ImadspRc_ASelect(o);
        case MXF_BSelect: return ImadspRc_BSelect(o);
        case MXF_CSelect: return ImadspRc_CSelect(o);
        }
        return MX_NO;
    case MXOP_ImnmxC:
        switch (f) {
        case MXF_Dest: return ImnmxC_Dest(o);
        case MXF_SrcA: return ImnmxC_SrcA(o);
        case MXF_CbufSlot: return ImnmxC_CbufSlot(o);
        case MXF_CbufOffset: return ImnmxC_CbufOffset(o);
        case MXF_Pred: return ImnmxC_Pred(o);
        case MXF_PredInv: return ImnmxC_PredInv(o);
        case MXF_WriteCC: return ImnmxC_WriteCC(o);
        case MXF_Signed: return ImnmxC_Signed(o);
        case MXF_XMode: return ImnmxC_XMode(o);
        case MXF_SrcPred: return ImnmxC_SrcPred(o);
        case MXF_SrcPredInv: return ImnmxC_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_ImnmxI:
        switch (f) {
        case MXF_Dest: return ImnmxI_Dest(o);
        case MXF_SrcA: return ImnmxI_SrcA(o);
        case MXF_Imm20: return ImnmxI_Imm20(o);
        case MXF_Pred: return ImnmxI_Pred(o);
        case MXF_PredInv: return ImnmxI_PredInv(o);
        case MXF_WriteCC: return ImnmxI_WriteCC(o);
        case MXF_Signed: return ImnmxI_Signed(o);
        case MXF_XMode: return ImnmxI_XMode(o);
        case MXF_SrcPred: return ImnmxI_SrcPred(o);
        case MXF_SrcPredInv: return ImnmxI_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_ImnmxR:
        switch (f) {
        case MXF_Dest: return ImnmxR_Dest(o);
        case MXF_SrcA: return ImnmxR_SrcA(o);
        case MXF_SrcB: return ImnmxR_SrcB(o);
        case MXF_Pred: return ImnmxR_Pred(o);
        case MXF_PredInv: return ImnmxR_PredInv(o);
        case MXF_WriteCC: return ImnmxR_WriteCC(o);
        case MXF_Signed: return ImnmxR_Signed(o);
        case MXF_XMode: return ImnmxR_XMode(o);
        case MXF_SrcPred: return ImnmxR_SrcPred(o);
        case MXF_SrcPredInv: return ImnmxR_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_Imul32i:
        switch (f) {
        case MXF_Dest: return Imul32i_Dest(o);
        case MXF_SrcA: return Imul32i_SrcA(o);
        case MXF_Pred: return Imul32i_Pred(o);
        case MXF_PredInv: return Imul32i_PredInv(o);
        case MXF_Imm32: return Imul32i_Imm32(o);
        case MXF_ASigned: return Imul32i_ASigned(o);
        case MXF_BSigned: return Imul32i_BSigned(o);
        case MXF_Hilo: return Imul32i_Hilo(o);
        case MXF_WriteCC: return Imul32i_WriteCC(o);
        }
        return MX_NO;
    case MXOP_ImulC:
        switch (f) {
        case MXF_Dest: return ImulC_Dest(o);
        case MXF_SrcA: return ImulC_SrcA(o);
        case MXF_CbufSlot: return ImulC_CbufSlot(o);
        case MXF_CbufOffset: return ImulC_CbufOffset(o);
        case MXF_Pred: return ImulC_Pred(o);
        case MXF_PredInv: return ImulC_PredInv(o);
        case MXF_WriteCC: return ImulC_WriteCC(o);
        case MXF_ASigned: return ImulC_ASigned(o);
        case MXF_BSigned: return ImulC_BSigned(o);
        case MXF_Hilo: return ImulC_Hilo(o);
        }
        return MX_NO;
    case MXOP_ImulI:
        switch (f) {
        case MXF_Dest: return ImulI_Dest(o);
        case MXF_SrcA: return ImulI_SrcA(o);
        case MXF_Imm20: return ImulI_Imm20(o);
        case MXF_Pred: return ImulI_Pred(o);
        case MXF_PredInv: return ImulI_PredInv(o);
        case MXF_WriteCC: return ImulI_WriteCC(o);
        case MXF_ASigned: return ImulI_ASigned(o);
        case MXF_BSigned: return ImulI_BSigned(o);
        case MXF_Hilo: return ImulI_Hilo(o);
        }
        return MX_NO;
    case MXOP_ImulR:
        switch (f) {
        case MXF_Dest: return ImulR_Dest(o);
        case MXF_SrcA: return ImulR_SrcA(o);
        case MXF_SrcB: return ImulR_SrcB(o);
        case MXF_Pred: return ImulR_Pred(o);
        case MXF_PredInv: return ImulR_PredInv(o);
        case MXF_WriteCC: return ImulR_WriteCC(o);
        case MXF_ASigned: return ImulR_ASigned(o);
        case MXF_BSigned: return ImulR_BSigned(o);
        case MXF_Hilo: return ImulR_Hilo(o);
        }
        return MX_NO;
    case MXOP_Ipa:
        switch (f) {
        case MXF_Dest: return Ipa_Dest(o);
        case MXF_SrcA: return Ipa_SrcA(o);
        case MXF_SrcB: return Ipa_SrcB(o);
        case MXF_SrcC: return Ipa_SrcC(o);
        case MXF_Pred: return Ipa_Pred(o);
        case MXF_PredInv: return Ipa_PredInv(o);
        case MXF_IpaOp: return Ipa_IpaOp(o);
        case MXF_Msi: return Ipa_Msi(o);
        case MXF_Sat: return Ipa_Sat(o);
        case MXF_Idx: return Ipa_Idx(o);
        case MXF_Imm10: return Ipa_Imm10(o);
        case MXF_SrcPred: return Ipa_SrcPred(o);
        case MXF_SrcPredInv: return Ipa_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_Isberd:
        switch (f) {
        case MXF_Dest: return Isberd_Dest(o);
        case MXF_SrcA: return Isberd_SrcA(o);
        case MXF_Pred: return Isberd_Pred(o);
        case MXF_PredInv: return Isberd_PredInv(o);
        case MXF_AlSize: return Isberd_AlSize(o);
        case MXF_IBase: return Isberd_IBase(o);
        case MXF_O: return Isberd_O(o);
        case MXF_P: return Isberd_P(o);
        }
        return MX_NO;
    case MXOP_Iscadd32i:
        switch (f) {
        case MXF_Dest: return Iscadd32i_Dest(o);
        case MXF_SrcA: return Iscadd32i_SrcA(o);
        case MXF_Pred: return Iscadd32i_Pred(o);
        case MXF_PredInv: return Iscadd32i_PredInv(o);
        case MXF_Imm32: return Iscadd32i_Imm32(o);
        case MXF_WriteCC: return Iscadd32i_WriteCC(o);
        case MXF_Imm5: return Iscadd32i_Imm5(o);
        }
        return MX_NO;
    case MXOP_IscaddC:
        switch (f) {
        case MXF_Dest: return IscaddC_Dest(o);
        case MXF_SrcA: return IscaddC_SrcA(o);
        case MXF_CbufSlot: return IscaddC_CbufSlot(o);
        case MXF_CbufOffset: return IscaddC_CbufOffset(o);
        case MXF_Pred: return IscaddC_Pred(o);
        case MXF_PredInv: return IscaddC_PredInv(o);
        case MXF_WriteCC: return IscaddC_WriteCC(o);
        case MXF_Imm5: return IscaddC_Imm5(o);
        case MXF_AvgMode: return IscaddC_AvgMode(o);
        }
        return MX_NO;
    case MXOP_IscaddI:
        switch (f) {
        case MXF_Dest: return IscaddI_Dest(o);
        case MXF_SrcA: return IscaddI_SrcA(o);
        case MXF_Imm20: return IscaddI_Imm20(o);
        case MXF_Pred: return IscaddI_Pred(o);
        case MXF_PredInv: return IscaddI_PredInv(o);
        case MXF_WriteCC: return IscaddI_WriteCC(o);
        case MXF_Imm5: return IscaddI_Imm5(o);
        case MXF_AvgMode: return IscaddI_AvgMode(o);
        }
        return MX_NO;
    case MXOP_IscaddR:
        switch (f) {
        case MXF_Dest: return IscaddR_Dest(o);
        case MXF_SrcA: return IscaddR_SrcA(o);
        case MXF_SrcB: return IscaddR_SrcB(o);
        case MXF_Pred: return IscaddR_Pred(o);
        case MXF_PredInv: return IscaddR_PredInv(o);
        case MXF_WriteCC: return IscaddR_WriteCC(o);
        case MXF_Imm5: return IscaddR_Imm5(o);
        case MXF_AvgMode: return IscaddR_AvgMode(o);
        }
        return MX_NO;
    case MXOP_IsetC:
        switch (f) {
        case MXF_Dest: return IsetC_Dest(o);
        case MXF_SrcA: return IsetC_SrcA(o);
        case MXF_CbufSlot: return IsetC_CbufSlot(o);
        case MXF_CbufOffset: return IsetC_CbufOffset(o);
        case MXF_Pred: return IsetC_Pred(o);
        case MXF_PredInv: return IsetC_PredInv(o);
        case MXF_WriteCC: return IsetC_WriteCC(o);
        case MXF_IComp: return IsetC_IComp(o);
        case MXF_Signed: return IsetC_Signed(o);
        case MXF_SrcPred: return IsetC_SrcPred(o);
        case MXF_SrcPredInv: return IsetC_SrcPredInv(o);
        case MXF_BVal: return IsetC_BVal(o);
        case MXF_Bop: return IsetC_Bop(o);
        case MXF_X: return IsetC_X(o);
        }
        return MX_NO;
    case MXOP_IsetI:
        switch (f) {
        case MXF_Dest: return IsetI_Dest(o);
        case MXF_SrcA: return IsetI_SrcA(o);
        case MXF_Imm20: return IsetI_Imm20(o);
        case MXF_Pred: return IsetI_Pred(o);
        case MXF_PredInv: return IsetI_PredInv(o);
        case MXF_WriteCC: return IsetI_WriteCC(o);
        case MXF_IComp: return IsetI_IComp(o);
        case MXF_Signed: return IsetI_Signed(o);
        case MXF_SrcPred: return IsetI_SrcPred(o);
        case MXF_SrcPredInv: return IsetI_SrcPredInv(o);
        case MXF_BVal: return IsetI_BVal(o);
        case MXF_Bop: return IsetI_Bop(o);
        case MXF_X: return IsetI_X(o);
        }
        return MX_NO;
    case MXOP_IsetR:
        switch (f) {
        case MXF_Dest: return IsetR_Dest(o);
        case MXF_SrcA: return IsetR_SrcA(o);
        case MXF_SrcB: return IsetR_SrcB(o);
        case MXF_Pred: return IsetR_Pred(o);
        case MXF_PredInv: return IsetR_PredInv(o);
        case MXF_WriteCC: return IsetR_WriteCC(o);
        case MXF_IComp: return IsetR_IComp(o);
        case MXF_Signed: return IsetR_Signed(o);
        case MXF_SrcPred: return IsetR_SrcPred(o);
        case MXF_SrcPredInv: return IsetR_SrcPredInv(o);
        case MXF_BVal: return IsetR_BVal(o);
        case MXF_Bop: return IsetR_Bop(o);
        case MXF_X: return IsetR_X(o);
        }
        return MX_NO;
    case MXOP_IsetpC:
        switch (f) {
        case MXF_SrcA: return IsetpC_SrcA(o);
        case MXF_CbufSlot: return IsetpC_CbufSlot(o);
        case MXF_CbufOffset: return IsetpC_CbufOffset(o);
        case MXF_Pred: return IsetpC_Pred(o);
        case MXF_PredInv: return IsetpC_PredInv(o);
        case MXF_IComp: return IsetpC_IComp(o);
        case MXF_Signed: return IsetpC_Signed(o);
        case MXF_Bop: return IsetpC_Bop(o);
        case MXF_X: return IsetpC_X(o);
        case MXF_SrcPred: return IsetpC_SrcPred(o);
        case MXF_SrcPredInv: return IsetpC_SrcPredInv(o);
        case MXF_DestPred: return IsetpC_DestPred(o);
        case MXF_DestPredInv: return IsetpC_DestPredInv(o);
        }
        return MX_NO;
    case MXOP_IsetpI:
        switch (f) {
        case MXF_SrcA: return IsetpI_SrcA(o);
        case MXF_Imm20: return IsetpI_Imm20(o);
        case MXF_Pred: return IsetpI_Pred(o);
        case MXF_PredInv: return IsetpI_PredInv(o);
        case MXF_IComp: return IsetpI_IComp(o);
        case MXF_Signed: return IsetpI_Signed(o);
        case MXF_Bop: return IsetpI_Bop(o);
        case MXF_X: return IsetpI_X(o);
        case MXF_SrcPred: return IsetpI_SrcPred(o);
        case MXF_SrcPredInv: return IsetpI_SrcPredInv(o);
        case MXF_DestPred: return IsetpI_DestPred(o);
        case MXF_DestPredInv: return IsetpI_DestPredInv(o);
        }
        return MX_NO;
    case MXOP_IsetpR:
        switch (f) {
        case MXF_SrcA: return IsetpR_SrcA(o);
        case MXF_SrcB: return IsetpR_SrcB(o);
        case MXF_Pred: return IsetpR_Pred(o);
        case MXF_PredInv: return IsetpR_PredInv(o);
        case MXF_IComp: return IsetpR_IComp(o);
        case MXF_Signed: return IsetpR_Signed(o);
        case MXF_Bop: return IsetpR_Bop(o);
        case MXF_X: return IsetpR_X(o);
        case MXF_SrcPred: return IsetpR_SrcPred(o);
        case MXF_SrcPredInv: return IsetpR_SrcPredInv(o);
        case MXF_DestPred: return IsetpR_DestPred(o);
        case MXF_DestPredInv: return IsetpR_DestPredInv(o);
        }
        return MX_NO;
    case MXOP_Jcal:
        switch (f) {
        case MXF_Imm32: return Jcal_Imm32(o);
        case MXF_Ca: return Jcal_Ca(o);
        case MXF_Inc: return Jcal_Inc(o);
        }
        return MX_NO;
    case MXOP_Jmp:
        switch (f) {
        case MXF_SrcA: return Jmp_SrcA(o);
        case MXF_Pred: return Jmp_Pred(o);
        case MXF_PredInv: return Jmp_PredInv(o);
        case MXF_Ccc: return Jmp_Ccc(o);
        case MXF_Ca: return Jmp_Ca(o);
        case MXF_Imm32: return Jmp_Imm32(o);
        case MXF_Lmt: return Jmp_Lmt(o);
        case MXF_U: return Jmp_U(o);
        }
        return MX_NO;
    case MXOP_Jmx:
        switch (f) {
        case MXF_SrcA: return Jmx_SrcA(o);
        case MXF_Pred: return Jmx_Pred(o);
        case MXF_PredInv: return Jmx_PredInv(o);
        case MXF_Ccc: return Jmx_Ccc(o);
        case MXF_Ca: return Jmx_Ca(o);
        case MXF_Imm32: return Jmx_Imm32(o);
        case MXF_Lmt: return Jmx_Lmt(o);
        }
        return MX_NO;
    case MXOP_Kil:
        switch (f) {
        case MXF_Pred: return Kil_Pred(o);
        case MXF_PredInv: return Kil_PredInv(o);
        case MXF_Ccc: return Kil_Ccc(o);
        }
        return MX_NO;
    case MXOP_Ld:
        switch (f) {
        case MXF_Dest: return Ld_Dest(o);
        case MXF_SrcA: return Ld_SrcA(o);
        case MXF_Pred: return Ld_Pred(o);
        case MXF_PredInv: return Ld_PredInv(o);
        case MXF_SrcPred: return Ld_SrcPred(o);
        case MXF_CacheOp: return Ld_CacheOp(o);
        case MXF_LsSize: return Ld_LsSize(o);
        case MXF_E: return Ld_E(o);
        case MXF_Imm32: return Ld_Imm32(o);
        }
        return MX_NO;
    case MXOP_Ldc:
        switch (f) {
        case MXF_Dest: return Ldc_Dest(o);
        case MXF_SrcA: return Ldc_SrcA(o);
        case MXF_Pred: return Ldc_Pred(o);
        case MXF_PredInv: return Ldc_PredInv(o);
        case MXF_LsSize: return Ldc_LsSize(o);
        case MXF_AddressMode: return Ldc_AddressMode(o);
        case MXF_CbufSlot: return Ldc_CbufSlot(o);
        case MXF_CbufOffset: return Ldc_CbufOffset(o);
        }
        return MX_NO;
    case MXOP_Ldg:
        switch (f) {
        case MXF_Dest: return Ldg_Dest(o);
        case MXF_SrcA: return Ldg_SrcA(o);
        case MXF_Pred: return Ldg_Pred(o);
        case MXF_PredInv: return Ldg_PredInv(o);
        case MXF_LsSize: return Ldg_LsSize(o);
        case MXF_CacheOp: return Ldg_CacheOp(o);
        case MXF_E: return Ldg_E(o);
        case MXF_Imm24: return Ldg_Imm24(o);
        }
        return MX_NO;
    case MXOP_Ldl:
        switch (f) {
        case MXF_Dest: return Ldl_Dest(o);
        case MXF_SrcA: return Ldl_SrcA(o);
        case MXF_Pred: return Ldl_Pred(o);
        case MXF_PredInv: return Ldl_PredInv(o);
        case MXF_LsSize: return Ldl_LsSize(o);
        case MXF_CacheOp: return Ldl_CacheOp(o);
        case MXF_Imm24: return Ldl_Imm24(o);
        }
        return MX_NO;
    case MXOP_Lds:
        switch (f) {
        case MXF_Dest: return Lds_Dest(o);
        case MXF_SrcA: return Lds_SrcA(o);
        case MXF_Pred: return Lds_Pred(o);
        case MXF_PredInv: return Lds_PredInv(o);
        case MXF_LsSize: return Lds_LsSize(o);
        case MXF_U: return Lds_U(o);
        case MXF_Imm24: return Lds_Imm24(o);
        }
        return MX_NO;
    case MXOP_LeaC:
        switch (f) {
        case MXF_Dest: return LeaC_Dest(o);
        case MXF_SrcA: return LeaC_SrcA(o);
        case MXF_CbufSlot: return LeaC_CbufSlot(o);
        case MXF_CbufOffset: return LeaC_CbufOffset(o);
        case MXF_Pred: return LeaC_Pred(o);
        case MXF_PredInv: return LeaC_PredInv(o);
        case MXF_WriteCC: return LeaC_WriteCC(o);
        case MXF_X: return LeaC_X(o);
        case MXF_NegA: return LeaC_NegA(o);
        case MXF_ImmU5: return LeaC_ImmU5(o);
        case MXF_DestPred: return LeaC_DestPred(o);
        }
        return MX_NO;
    case MXOP_LeaHiC:
        switch (f) {
        case MXF_Dest: return LeaHiC_Dest(o);
        case MXF_SrcA: return LeaHiC_SrcA(o);
        case MXF_CbufSlot: return LeaHiC_CbufSlot(o);
        case MXF_CbufOffset: return LeaHiC_CbufOffset(o);
        case MXF_SrcC: return LeaHiC_SrcC(o);
        case MXF_Pred: return LeaHiC_Pred(o);
        case MXF_PredInv: return LeaHiC_PredInv(o);
        case MXF_WriteCC: return LeaHiC_WriteCC(o);
        case MXF_X: return LeaHiC_X(o);
        case MXF_NegA: return LeaHiC_NegA(o);
        case MXF_ImmU5: return LeaHiC_ImmU5(o);
        case MXF_DestPred: return LeaHiC_DestPred(o);
        }
        return MX_NO;
    case MXOP_LeaHiR:
        switch (f) {
        case MXF_Dest: return LeaHiR_Dest(o);
        case MXF_SrcA: return LeaHiR_SrcA(o);
        case MXF_SrcB: return LeaHiR_SrcB(o);
        case MXF_SrcC: return LeaHiR_SrcC(o);
        case MXF_Pred: return LeaHiR_Pred(o);
        case MXF_PredInv: return LeaHiR_PredInv(o);
        case MXF_WriteCC: return LeaHiR_WriteCC(o);
        case MXF_X: return LeaHiR_X(o);
        case MXF_NegA: return LeaHiR_NegA(o);
        case MXF_ImmU5: return LeaHiR_ImmU5(o);
        case MXF_DestPred: return LeaHiR_DestPred(o);
        }
        return MX_NO;
    case MXOP_LeaI:
        switch (f) {
        case MXF_Dest: return LeaI_Dest(o);
        case MXF_SrcA: return LeaI_SrcA(o);
        case MXF_Imm20: return LeaI_Imm20(o);
        case MXF_Pred: return LeaI_Pred(o);
        case MXF_PredInv: return LeaI_PredInv(o);
        case MXF_WriteCC: return LeaI_WriteCC(o);
        case MXF_X: return LeaI_X(o);
        case MXF_NegA: return LeaI_NegA(o);
        case MXF_ImmU5: return LeaI_ImmU5(o);
        case MXF_DestPred: return LeaI_DestPred(o);
        }
        return MX_NO;
    case MXOP_LeaR:
        switch (f) {
        case MXF_Dest: return LeaR_Dest(o);
        case MXF_SrcA: return LeaR_SrcA(o);
        case MXF_SrcB: return LeaR_SrcB(o);
        case MXF_Pred: return LeaR_Pred(o);
        case MXF_PredInv: return LeaR_PredInv(o);
        case MXF_WriteCC: return LeaR_WriteCC(o);
        case MXF_X: return LeaR_X(o);
        case MXF_NegA: return LeaR_NegA(o);
        case MXF_ImmU5: return LeaR_ImmU5(o);
        case MXF_DestPred: return LeaR_DestPred(o);
        }
        return MX_NO;
    case MXOP_Longjmp:
        switch (f) {
        case MXF_Ccc: return Longjmp_Ccc(o);
        }
        return MX_NO;
    case MXOP_Lop32i:
        switch (f) {
        case MXF_Dest: return Lop32i_Dest(o);
        case MXF_SrcA: return Lop32i_SrcA(o);
        case MXF_Pred: return Lop32i_Pred(o);
        case MXF_PredInv: return Lop32i_PredInv(o);
        case MXF_WriteCC: return Lop32i_WriteCC(o);
        case MXF_Imm32: return Lop32i_Imm32(o);
        case MXF_X: return Lop32i_X(o);
        case MXF_LogicOp: return Lop32i_LogicOp(o);
        case MXF_NegA: return Lop32i_NegA(o);
        case MXF_NegB: return Lop32i_NegB(o);
        }
        return MX_NO;
    case MXOP_Lop3C:
        switch (f) {
        case MXF_Dest: return Lop3C_Dest(o);
        case MXF_SrcA: return Lop3C_SrcA(o);
        case MXF_CbufSlot: return Lop3C_CbufSlot(o);
        case MXF_CbufOffset: return Lop3C_CbufOffset(o);
        case MXF_SrcC: return Lop3C_SrcC(o);
        case MXF_Pred: return Lop3C_Pred(o);
        case MXF_PredInv: return Lop3C_PredInv(o);
        case MXF_WriteCC: return Lop3C_WriteCC(o);
        case MXF_X: return Lop3C_X(o);
        case MXF_Imm: return Lop3C_Imm(o);
        }
        return MX_NO;
    case MXOP_Lop3I:
        switch (f) {
        case MXF_Dest: return Lop3I_Dest(o);
        case MXF_SrcA: return Lop3I_SrcA(o);
        case MXF_Imm20: return Lop3I_Imm20(o);
        case MXF_SrcC: return Lop3I_SrcC(o);
        case MXF_Pred: return Lop3I_Pred(o);
        case MXF_PredInv: return Lop3I_PredInv(o);
        case MXF_WriteCC: return Lop3I_WriteCC(o);
        case MXF_X: return Lop3I_X(o);
        case MXF_Imm: return Lop3I_Imm(o);
        }
        return MX_NO;
    case MXOP_Lop3R:
        switch (f) {
        case MXF_Dest: return Lop3R_Dest(o);
        case MXF_SrcA: return Lop3R_SrcA(o);
        case MXF_SrcB: return Lop3R_SrcB(o);
        case MXF_SrcC: return Lop3R_SrcC(o);
        case MXF_Pred: return Lop3R_Pred(o);
        case MXF_PredInv: return Lop3R_PredInv(o);
        case MXF_WriteCC: return Lop3R_WriteCC(o);
        case MXF_DestPred: return Lop3R_DestPred(o);
        case MXF_PredicateOp: return Lop3R_PredicateOp(o);
        case MXF_X: return Lop3R_X(o);
        case MXF_Imm: return Lop3R_Imm(o);
        }
        return MX_NO;
    case MXOP_LopC:
        switch (f) {
        case MXF_Dest: return LopC_Dest(o);
        case MXF_SrcA: return LopC_SrcA(o);
        case MXF_CbufSlot: return LopC_CbufSlot(o);
        case MXF_CbufOffset: return LopC_CbufOffset(o);
        case MXF_Pred: return LopC_Pred(o);
        case MXF_PredInv: return LopC_PredInv(o);
        case MXF_WriteCC: return LopC_WriteCC(o);
        case MXF_DestPred: return LopC_DestPred(o);
        case MXF_PredicateOp: return LopC_PredicateOp(o);
        case MXF_X: return LopC_X(o);
        case MXF_LogicOp: return LopC_LogicOp(o);
        case MXF_NegA: return LopC_NegA(o);
        case MXF_NegB: return LopC_NegB(o);
        }
        return MX_NO;
    case MXOP_LopI:
        switch (f) {
        case MXF_Dest: return LopI_Dest(o);
        case MXF_SrcA: return LopI_SrcA(o);
        case MXF_Imm20: return LopI_Imm20(o);
        case MXF_Pred: return LopI_Pred(o);
        case MXF_PredInv: return LopI_PredInv(o);
        case MXF_WriteCC: return LopI_WriteCC(o);
        case MXF_DestPred: return LopI_DestPred(o);
        case MXF_PredicateOp: return LopI_PredicateOp(o);
        case MXF_X: return LopI_X(o);
        case MXF_LogicOp: return LopI_LogicOp(o);
        case MXF_NegA: return LopI_NegA(o);
        case MXF_NegB: return LopI_NegB(o);
        }
        return MX_NO;
    case MXOP_LopR:
        switch (f) {
        case MXF_Dest: return LopR_Dest(o);
        case MXF_SrcA: return LopR_SrcA(o);
        case MXF_SrcB: return LopR_SrcB(o);
        case MXF_Pred: return LopR_Pred(o);
        case MXF_PredInv: return LopR_PredInv(o);
        case MXF_WriteCC: return LopR_WriteCC(o);
        case MXF_DestPred: return LopR_DestPred(o);
        case MXF_PredicateOp: return LopR_PredicateOp(o);
        case MXF_X: return LopR_X(o);
        case MXF_Lop: return LopR_Lop(o);
        case MXF_NegA: return LopR_NegA(o);
        case MXF_NegB: return LopR_NegB(o);
        }
        return MX_NO;
    case MXOP_Membar:
        switch (f) {
        case MXF_Pred: return Membar_Pred(o);
        case MXF_PredInv: return Membar_PredInv(o);
        case MXF_Membar: return Membar_Membar(o);
        case MXF_Ivall: return Membar_Ivall(o);
        }
        return MX_NO;
    case MXOP_Mov32i:
        switch (f) {
        case MXF_Dest: return Mov32i_Dest(o);
        case MXF_Imm32: return Mov32i_Imm32(o);
        case MXF_Pred: return Mov32i_Pred(o);
        case MXF_PredInv: return Mov32i_PredInv(o);
        case MXF_QuadMask: return Mov32i_QuadMask(o);
        }
        return MX_NO;
    case MXOP_MovC:
        switch (f) {
        case MXF_Dest: return MovC_Dest(o);
        case MXF_CbufSlot: return MovC_CbufSlot(o);
        case MXF_CbufOffset: return MovC_CbufOffset(o);
        case MXF_Pred: return MovC_Pred(o);
        case MXF_PredInv: return MovC_PredInv(o);
        case MXF_QuadMask: return MovC_QuadMask(o);
        }
        return MX_NO;
    case MXOP_MovI:
        switch (f) {
        case MXF_Dest: return MovI_Dest(o);
        case MXF_Imm20: return MovI_Imm20(o);
        case MXF_Pred: return MovI_Pred(o);
        case MXF_PredInv: return MovI_PredInv(o);
        case MXF_QuadMask: return MovI_QuadMask(o);
        }
        return MX_NO;
    case MXOP_MovR:
        switch (f) {
        case MXF_Dest: return MovR_Dest(o);
        case MXF_SrcA: return MovR_SrcA(o);
        case MXF_Pred: return MovR_Pred(o);
        case MXF_PredInv: return MovR_PredInv(o);
        case MXF_QuadMask: return MovR_QuadMask(o);
        }
        return MX_NO;
    case MXOP_Mufu:
        switch (f) {
        case MXF_Dest: return Mufu_Dest(o);
        case MXF_SrcA: return Mufu_SrcA(o);
        case MXF_Pred: return Mufu_Pred(o);
        case MXF_PredInv: return Mufu_PredInv(o);
        case MXF_MufuOp: return Mufu_MufuOp(o);
        case MXF_AbsA: return Mufu_AbsA(o);
        case MXF_NegA: return Mufu_NegA(o);
        case MXF_Sat: return Mufu_Sat(o);
        }
        return MX_NO;
    case MXOP_Nop:
        switch (f) {
        case MXF_Pred: return Nop_Pred(o);
        case MXF_PredInv: return Nop_PredInv(o);
        case MXF_Imm16: return Nop_Imm16(o);
        case MXF_Trig: return Nop_Trig(o);
        case MXF_Ccc: return Nop_Ccc(o);
        }
        return MX_NO;
    case MXOP_OutC:
        switch (f) {
        case MXF_Dest: return OutC_Dest(o);
        case MXF_SrcA: return OutC_SrcA(o);
        case MXF_CbufSlot: return OutC_CbufSlot(o);
        case MXF_CbufOffset: return OutC_CbufOffset(o);
        case MXF_Pred: return OutC_Pred(o);
        case MXF_PredInv: return OutC_PredInv(o);
        case MXF_OutType: return OutC_OutType(o);
        }
        return MX_NO;
    case MXOP_OutI:
        switch (f) {
        case MXF_Dest: return OutI_Dest(o);
        case MXF_SrcA: return OutI_SrcA(o);
        case MXF_Imm20: return OutI_Imm20(o);
        case MXF_Pred: return OutI_Pred(o);
        case MXF_PredInv: return OutI_PredInv(o);
        case MXF_OutType: return OutI_OutType(o);
        }
        return MX_NO;
    case MXOP_OutR:
        switch (f) {
        case MXF_Dest: return OutR_Dest(o);
        case MXF_SrcA: return OutR_SrcA(o);
        case MXF_SrcB: return OutR_SrcB(o);
        case MXF_Pred: return OutR_Pred(o);
        case MXF_PredInv: return OutR_PredInv(o);
        case MXF_OutType: return OutR_OutType(o);
        }
        return MX_NO;
    case MXOP_P2rC:
        switch (f) {
        case MXF_Dest: return P2rC_Dest(o);
        case MXF_SrcA: return P2rC_SrcA(o);
        case MXF_CbufSlot: return P2rC_CbufSlot(o);
        case MXF_CbufOffset: return P2rC_CbufOffset(o);
        case MXF_Pred: return P2rC_Pred(o);
        case MXF_PredInv: return P2rC_PredInv(o);
        case MXF_ByteSel: return P2rC_ByteSel(o);
        case MXF_Ccpr: return P2rC_Ccpr(o);
        }
        return MX_NO;
    case MXOP_P2rI:
        switch (f) {
        case MXF_Dest: return P2rI_Dest(o);
        case MXF_SrcA: return P2rI_SrcA(o);
        case MXF_Imm20: return P2rI_Imm20(o);
        case MXF_Pred: return P2rI_Pred(o);
        case MXF_PredInv: return P2rI_PredInv(o);
        case MXF_ByteSel: return P2rI_ByteSel(o);
        case MXF_Ccpr: return P2rI_Ccpr(o);
        }
        return MX_NO;
    case MXOP_P2rR:
        switch (f) {
        case MXF_Dest: return P2rR_Dest(o);
        case MXF_SrcA: return P2rR_SrcA(o);
        case MXF_SrcB: return P2rR_SrcB(o);
        case MXF_Pred: return P2rR_Pred(o);
        case MXF_PredInv: return P2rR_PredInv(o);
        case MXF_ByteSel: return P2rR_ByteSel(o);
        case MXF_Ccpr: return P2rR_Ccpr(o);
        }
        return MX_NO;
    case MXOP_Pbk:
        switch (f) {
        case MXF_Imm24: return Pbk_Imm24(o);
        case MXF_Ca: return Pbk_Ca(o);
        }
        return MX_NO;
    case MXOP_Pcnt:
        switch (f) {
        case MXF_Imm24: return Pcnt_Imm24(o);
        case MXF_Ca: return Pcnt_Ca(o);
        }
        return MX_NO;
    case MXOP_Pexit:
        switch (f) {
        case MXF_Imm24: return Pexit_Imm24(o);
        }
        return MX_NO;
    case MXOP_Pixld:
        switch (f) {
        case MXF_Dest: return Pixld_Dest(o);
        case MXF_SrcA: return Pixld_SrcA(o);
        case MXF_Pred: return Pixld_Pred(o);
        case MXF_PredInv: return Pixld_PredInv(o);
        case MXF_DestPred: return Pixld_DestPred(o);
        case MXF_PixMode: return Pixld_PixMode(o);
        case MXF_Imm8: return Pixld_Imm8(o);
        }
        return MX_NO;
    case MXOP_Plongjmp:
        switch (f) {
        case MXF_Imm24: return Plongjmp_Imm24(o);
        case MXF_Ca: return Plongjmp_Ca(o);
        }
        return MX_NO;
    case MXOP_PopcC:
        switch (f) {
        case MXF_Dest: return PopcC_Dest(o);
        case MXF_CbufSlot: return PopcC_CbufSlot(o);
        case MXF_CbufOffset: return PopcC_CbufOffset(o);
        case MXF_Pred: return PopcC_Pred(o);
        case MXF_PredInv: return PopcC_PredInv(o);
        case MXF_NegB: return PopcC_NegB(o);
        }
        return MX_NO;
    case MXOP_PopcI:
        switch (f) {
        case MXF_Dest: return PopcI_Dest(o);
        case MXF_Imm20: return PopcI_Imm20(o);
        case MXF_Pred: return PopcI_Pred(o);
        case MXF_PredInv: return PopcI_PredInv(o);
        case MXF_NegB: return PopcI_NegB(o);
        }
        return MX_NO;
    case MXOP_PopcR:
        switch (f) {
        case MXF_Dest: return PopcR_Dest(o);
        case MXF_SrcB: return PopcR_SrcB(o);
        case MXF_Pred: return PopcR_Pred(o);
        case MXF_PredInv: return PopcR_PredInv(o);
        case MXF_NegB: return PopcR_NegB(o);
        }
        return MX_NO;
    case MXOP_Pret:
        switch (f) {
        case MXF_Ca: return Pret_Ca(o);
        case MXF_Imm24: return Pret_Imm24(o);
        case MXF_Inc: return Pret_Inc(o);
        }
        return MX_NO;
    case MXOP_PrmtC:
        switch (f) {
        case MXF_Dest: return PrmtC_Dest(o);
        case MXF_SrcA: return PrmtC_SrcA(o);
        case MXF_CbufSlot: return PrmtC_CbufSlot(o);
        case MXF_CbufOffset: return PrmtC_CbufOffset(o);
        case MXF_SrcC: return PrmtC_SrcC(o);
        case MXF_Pred: return PrmtC_Pred(o);
        case MXF_PredInv: return PrmtC_PredInv(o);
        case MXF_PMode: return PrmtC_PMode(o);
        }
        return MX_NO;
    case MXOP_PrmtI:
        switch (f) {
        case MXF_Dest: return PrmtI_Dest(o);
        case MXF_SrcA: return PrmtI_SrcA(o);
        case MXF_Imm20: return PrmtI_Imm20(o);
        case MXF_SrcC: return PrmtI_SrcC(o);
        case MXF_Pred: return PrmtI_Pred(o);
        case MXF_PredInv: return PrmtI_PredInv(o);
        case MXF_PMode: return PrmtI_PMode(o);
        }
        return MX_NO;
    case MXOP_PrmtR:
        switch (f) {
        case MXF_Dest: return PrmtR_Dest(o);
        case MXF_SrcA: return PrmtR_SrcA(o);
        case MXF_SrcB: return PrmtR_SrcB(o);
        case MXF_SrcC: return PrmtR_SrcC(o);
        case MXF_Pred: return PrmtR_Pred(o);
        case MXF_PredInv: return PrmtR_PredInv(o);
        case MXF_PMode: return PrmtR_PMode(o);
        }
        return MX_NO;
    case MXOP_PrmtRc:
        switch (f) {
        case MXF_Dest: return PrmtRc_Dest(o);
        case MXF_SrcA: return PrmtRc_SrcA(o);
        case MXF_SrcC: return PrmtRc_SrcC(o);
        case MXF_CbufSlot: return PrmtRc_CbufSlot(o);
        case MXF_CbufOffset: return PrmtRc_CbufOffset(o);
        case MXF_Pred: return PrmtRc_Pred(o);
        case MXF_PredInv: return PrmtRc_PredInv(o);
        case MXF_PMode: return PrmtRc_PMode(o);
        }
        return MX_NO;
    case MXOP_Pset:
        switch (f) {
        case MXF_Dest: return Pset_Dest(o);
        case MXF_Pred: return Pset_Pred(o);
        case MXF_PredInv: return Pset_PredInv(o);
        case MXF_WriteCC: return Pset_WriteCC(o);
        case MXF_Src2Pred: return Pset_Src2Pred(o);
        case MXF_Src2PredInv: return Pset_Src2PredInv(o);
        case MXF_Src1Pred: return Pset_Src1Pred(o);
        case MXF_Src1PredInv: return Pset_Src1PredInv(o);
        case MXF_SrcPred: return Pset_SrcPred(o);
        case MXF_SrcPredInv: return Pset_SrcPredInv(o);
        case MXF_BoolOpAB: return Pset_BoolOpAB(o);
        case MXF_BoolOpC: return Pset_BoolOpC(o);
        case MXF_BVal: return Pset_BVal(o);
        }
        return MX_NO;
    case MXOP_Psetp:
        switch (f) {
        case MXF_DestPred: return Psetp_DestPred(o);
        case MXF_Pred: return Psetp_Pred(o);
        case MXF_PredInv: return Psetp_PredInv(o);
        case MXF_DestPredInv: return Psetp_DestPredInv(o);
        case MXF_Src2Pred: return Psetp_Src2Pred(o);
        case MXF_Src2PredInv: return Psetp_Src2PredInv(o);
        case MXF_Src1Pred: return Psetp_Src1Pred(o);
        case MXF_Src1PredInv: return Psetp_Src1PredInv(o);
        case MXF_SrcPred: return Psetp_SrcPred(o);
        case MXF_SrcPredInv: return Psetp_SrcPredInv(o);
        case MXF_BoolOpAB: return Psetp_BoolOpAB(o);
        case MXF_BoolOpC: return Psetp_BoolOpC(o);
        }
        return MX_NO;
    case MXOP_R2b:
        switch (f) {
        case MXF_SrcB: return R2b_SrcB(o);
        case MXF_Pred: return R2b_Pred(o);
        case MXF_PredInv: return R2b_PredInv(o);
        case MXF_Mode: return R2b_Mode(o);
        case MXF_Name: return R2b_Name(o);
        }
        return MX_NO;
    case MXOP_R2pC:
        switch (f) {
        case MXF_SrcA: return R2pC_SrcA(o);
        case MXF_CbufSlot: return R2pC_CbufSlot(o);
        case MXF_CbufOffset: return R2pC_CbufOffset(o);
        case MXF_Pred: return R2pC_Pred(o);
        case MXF_PredInv: return R2pC_PredInv(o);
        case MXF_ByteSel: return R2pC_ByteSel(o);
        case MXF_Ccpr: return R2pC_Ccpr(o);
        }
        return MX_NO;
    case MXOP_R2pI:
        switch (f) {
        case MXF_SrcA: return R2pI_SrcA(o);
        case MXF_Imm20: return R2pI_Imm20(o);
        case MXF_Pred: return R2pI_Pred(o);
        case MXF_PredInv: return R2pI_PredInv(o);
        case MXF_ByteSel: return R2pI_ByteSel(o);
        case MXF_Ccpr: return R2pI_Ccpr(o);
        }
        return MX_NO;
    case MXOP_R2pR:
        switch (f) {
        case MXF_SrcA: return R2pR_SrcA(o);
        case MXF_SrcB: return R2pR_SrcB(o);
        case MXF_Pred: return R2pR_Pred(o);
        case MXF_PredInv: return R2pR_PredInv(o);
        case MXF_ByteSel: return R2pR_ByteSel(o);
        case MXF_Ccpr: return R2pR_Ccpr(o);
        }
        return MX_NO;
    case MXOP_Red:
        switch (f) {
        case MXF_SrcA: return Red_SrcA(o);
        case MXF_SrcB: return Red_SrcB(o);
        case MXF_Pred: return Red_Pred(o);
        case MXF_PredInv: return Red_PredInv(o);
        case MXF_Imm20: return Red_Imm20(o);
        case MXF_RedSize: return Red_RedSize(o);
        case MXF_RedOp: return Red_RedOp(o);
        case MXF_E: return Red_E(o);
        }
        return MX_NO;
    case MXOP_Ret:
        switch (f) {
        case MXF_Pred: return Ret_Pred(o);
        case MXF_PredInv: return Ret_PredInv(o);
        case MXF_Ccc: return Ret_Ccc(o);
        }
        return MX_NO;
    case MXOP_RroC:
        switch (f) {
        case MXF_Dest: return RroC_Dest(o);
        case MXF_CbufSlot: return RroC_CbufSlot(o);
        case MXF_CbufOffset: return RroC_CbufOffset(o);
        case MXF_Pred: return RroC_Pred(o);
        case MXF_PredInv: return RroC_PredInv(o);
        case MXF_AbsB: return RroC_AbsB(o);
        case MXF_NegB: return RroC_NegB(o);
        case MXF_RroOp: return RroC_RroOp(o);
        }
        return MX_NO;
    case MXOP_RroI:
        switch (f) {
        case MXF_Dest: return RroI_Dest(o);
        case MXF_Imm20: return RroI_Imm20(o);
        case MXF_Pred: return RroI_Pred(o);
        case MXF_PredInv: return RroI_PredInv(o);
        case MXF_AbsB: return RroI_AbsB(o);
        case MXF_NegB: return RroI_NegB(o);
        case MXF_RroOp: return RroI_RroOp(o);
        }
        return MX_NO;
    case MXOP_RroR:
        switch (f) {
        case MXF_Dest: return RroR_Dest(o);
        case MXF_SrcB: return RroR_SrcB(o);
        case MXF_Pred: return RroR_Pred(o);
        case MXF_PredInv: return RroR_PredInv(o);
        case MXF_AbsB: return RroR_AbsB(o);
        case MXF_NegB: return RroR_NegB(o);
        case MXF_RroOp: return RroR_RroOp(o);
        }
        return MX_NO;
    case MXOP_S2r:
        switch (f) {
        case MXF_Dest: return S2r_Dest(o);
        case MXF_Pred: return S2r_Pred(o);
        case MXF_PredInv: return S2r_PredInv(o);
        case MXF_SReg: return S2r_SReg(o);
        }
        return MX_NO;
    case MXOP_SelC:
        switch (f) {
        case MXF_Dest: return SelC_Dest(o);
        case MXF_SrcA: return SelC_SrcA(o);
        case MXF_CbufSlot: return SelC_CbufSlot(o);
        case MXF_CbufOffset: return SelC_CbufOffset(o);
        case MXF_Pred: return SelC_Pred(o);
        case MXF_PredInv: return SelC_PredInv(o);
        case MXF_SrcPred: return SelC_SrcPred(o);
        case MXF_SrcPredInv: return SelC_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_SelI:
        switch (f) {
        case MXF_Dest: return SelI_Dest(o);
        case MXF_SrcA: return SelI_SrcA(o);
        case MXF_Imm20: return SelI_Imm20(o);
        case MXF_Pred: return SelI_Pred(o);
        case MXF_PredInv: return SelI_PredInv(o);
        case MXF_SrcPred: return SelI_SrcPred(o);
        case MXF_SrcPredInv: return SelI_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_SelR:
        switch (f) {
        case MXF_Dest: return SelR_Dest(o);
        case MXF_SrcA: return SelR_SrcA(o);
        case MXF_SrcB: return SelR_SrcB(o);
        case MXF_Pred: return SelR_Pred(o);
        case MXF_PredInv: return SelR_PredInv(o);
        case MXF_SrcPred: return SelR_SrcPred(o);
        case MXF_SrcPredInv: return SelR_SrcPredInv(o);
        }
        return MX_NO;
    case MXOP_Setcrsptr:
        switch (f) {
        case MXF_SrcA: return Setcrsptr_SrcA(o);
        }
        return MX_NO;
    case MXOP_Setlmembase:
        switch (f) {
        case MXF_SrcA: return Setlmembase_SrcA(o);
        }
        return MX_NO;
    case MXOP_ShfLI:
        switch (f) {
        case MXF_Dest: return ShfLI_Dest(o);
        case MXF_SrcA: return ShfLI_SrcA(o);
        case MXF_SrcC: return ShfLI_SrcC(o);
        case MXF_Pred: return ShfLI_Pred(o);
        case MXF_PredInv: return ShfLI_PredInv(o);
        case MXF_WriteCC: return ShfLI_WriteCC(o);
        case MXF_M: return ShfLI_M(o);
        case MXF_XModeShf: return ShfLI_XModeShf(o);
        case MXF_MaxShift: return ShfLI_MaxShift(o);
        case MXF_Imm6: return ShfLI_Imm6(o);
        }
        return MX_NO;
    case MXOP_ShfLR:
        switch (f) {
        case MXF_Dest: return ShfLR_Dest(o);
        case MXF_SrcA: return ShfLR_SrcA(o);
        case MXF_SrcB: return ShfLR_SrcB(o);
        case MXF_SrcC: return ShfLR_SrcC(o);
        case MXF_Pred: return ShfLR_Pred(o);
        case MXF_PredInv: return ShfLR_PredInv(o);
        case MXF_WriteCC: return ShfLR_WriteCC(o);
        case MXF_M: return ShfLR_M(o);
        case MXF_XModeShf: return ShfLR_XModeShf(o);
        case MXF_MaxShift: return ShfLR_MaxShift(o);
        }
        return MX_NO;
    case MXOP_ShfRI:
        switch (f) {
        case MXF_Dest: return ShfRI_Dest(o);
        case MXF_SrcA: return ShfRI_SrcA(o);
        case MXF_SrcC: return ShfRI_SrcC(o);
        case MXF_Pred: return ShfRI_Pred(o);
        case MXF_PredInv: return ShfRI_PredInv(o);
        case MXF_WriteCC: return ShfRI_WriteCC(o);
        case MXF_M: return ShfRI_M(o);
        case MXF_XModeShf: return ShfRI_XModeShf(o);
        case MXF_MaxShift: return ShfRI_MaxShift(o);
        case MXF_Imm6: return ShfRI_Imm6(o);
        }
        return MX_NO;
    case MXOP_ShfRR:
        switch (f) {
        case MXF_Dest: return ShfRR_Dest(o);
        case MXF_SrcA: return ShfRR_SrcA(o);
        case MXF_SrcB: return ShfRR_SrcB(o);
        case MXF_SrcC: return ShfRR_SrcC(o);
        case MXF_Pred: return ShfRR_Pred(o);
        case MXF_PredInv: return ShfRR_PredInv(o);
        case MXF_WriteCC: return ShfRR_WriteCC(o);
        case MXF_M: return ShfRR_M(o);
        case MXF_XModeShf: return ShfRR_XModeShf(o);
        case MXF_MaxShift: return ShfRR_MaxShift(o);
        }
        return MX_NO;
    case MXOP_Shfl:
        switch (f) {
        case MXF_Dest: return Shfl_Dest(o);
        case MXF_SrcA: return Shfl_SrcA(o);
        case MXF_SrcB: return Shfl_SrcB(o);
        case MXF_SrcC: return Shfl_SrcC(o);
        case MXF_SrcBImm: return Shfl_SrcBImm(o);
        case MXF_SrcCImm: return Shfl_SrcCImm(o);
        case MXF_Pred: return Shfl_Pred(o);
        case MXF_PredInv: return Shfl_PredInv(o);
        case MXF_ShflMode: return Shfl_ShflMode(o);
        case MXF_CFixShfl: return Shfl_CFixShfl(o);
        case MXF_BFixShfl: return Shfl_BFixShfl(o);
        case MXF_DestPred: return Shfl_DestPred(o);
        }
        return MX_NO;
    case MXOP_ShlC:
        switch (f) {
        case MXF_Dest: return ShlC_Dest(o);
        case MXF_SrcA: return ShlC_SrcA(o);
        case MXF_CbufSlot: return ShlC_CbufSlot(o);
        case MXF_CbufOffset: return ShlC_CbufOffset(o);
        case MXF_Pred: return ShlC_Pred(o);
        case MXF_PredInv: return ShlC_PredInv(o);
        case MXF_WriteCC: return ShlC_WriteCC(o);
        case MXF_X: return ShlC_X(o);
        case MXF_M: return ShlC_M(o);
        }
        return MX_NO;
    case MXOP_ShlI:
        switch (f) {
        case MXF_Dest: return ShlI_Dest(o);
        case MXF_SrcA: return ShlI_SrcA(o);
        case MXF_Imm20: return ShlI_Imm20(o);
        case MXF_Pred: return ShlI_Pred(o);
        case MXF_PredInv: return ShlI_PredInv(o);
        case MXF_WriteCC: return ShlI_WriteCC(o);
        case MXF_X: return ShlI_X(o);
        case MXF_M: return ShlI_M(o);
        }
        return MX_NO;
    case MXOP_ShlR:
        switch (f) {
        case MXF_Dest: return ShlR_Dest(o);
        case MXF_SrcA: return ShlR_SrcA(o);
        case MXF_SrcB: return ShlR_SrcB(o);
        case MXF_Pred: return ShlR_Pred(o);
        case MXF_PredInv: return ShlR_PredInv(o);
        case MXF_WriteCC: return ShlR_WriteCC(o);
        case MXF_X: return ShlR_X(o);
        case MXF_M: return ShlR_M(o);
        }
        return MX_NO;
    case MXOP_ShrC:
        switch (f) {
        case MXF_Dest: return ShrC_Dest(o);
        case MXF_SrcA: return ShrC_SrcA(o);
        case MXF_CbufSlot: return ShrC_CbufSlot(o);
        case MXF_CbufOffset: return ShrC_CbufOffset(o);
        case MXF_Pred: return ShrC_Pred(o);
        case MXF_PredInv: return ShrC_PredInv(o);
        case MXF_WriteCC: return ShrC_WriteCC(o);
        case MXF_Signed: return ShrC_Signed(o);
        case MXF_XMode: return ShrC_XMode(o);
        case MXF_Brev: return ShrC_Brev(o);
        case MXF_M: return ShrC_M(o);
        }
        return MX_NO;
    case MXOP_ShrI:
        switch (f) {
        case MXF_Dest: return ShrI_Dest(o);
        case MXF_SrcA: return ShrI_SrcA(o);
        case MXF_Imm20: return ShrI_Imm20(o);
        case MXF_Pred: return ShrI_Pred(o);
        case MXF_PredInv: return ShrI_PredInv(o);
        case MXF_WriteCC: return ShrI_WriteCC(o);
        case MXF_Signed: return ShrI_Signed(o);
        case MXF_XMode: return ShrI_XMode(o);
        case MXF_Brev: return ShrI_Brev(o);
        case MXF_M: return ShrI_M(o);
        }
        return MX_NO;
    case MXOP_ShrR:
        switch (f) {
        case MXF_Dest: return ShrR_Dest(o);
        case MXF_SrcA: return ShrR_SrcA(o);
        case MXF_SrcB: return ShrR_SrcB(o);
        case MXF_Pred: return ShrR_Pred(o);
        case MXF_PredInv: return ShrR_PredInv(o);
        case MXF_WriteCC: return ShrR_WriteCC(o);
        case MXF_Signed: return ShrR_Signed(o);
        case MXF_XMode: return ShrR_XMode(o);
        case MXF_Brev: return ShrR_Brev(o);
        case MXF_M: return ShrR_M(o);
        }
        return MX_NO;
    case MXOP_Ssy:
        switch (f) {
        case MXF_Imm24: return Ssy_Imm24(o);
        case MXF_Ca: return Ssy_Ca(o);
        }
        return MX_NO;
    case MXOP_St:
        switch (f) {
        case MXF_Dest: return St_Dest(o);
        case MXF_SrcA: return St_SrcA(o);
        case MXF_Pred: return St_Pred(o);
        case MXF_PredInv: return St_PredInv(o);
        case MXF_SrcPred: return St_SrcPred(o);
        case MXF_CacheOp: return St_CacheOp(o);
        case MXF_LsSize: return St_LsSize(o);
        case MXF_E: return St_E(o);
        case MXF_Imm32: return St_Imm32(o);
        }
        return MX_NO;
    case MXOP_Stg:
        switch (f) {
        case MXF_Dest: return Stg_Dest(o);
        case MXF_SrcA: return Stg_SrcA(o);
        case MXF_Pred: return Stg_Pred(o);
        case MXF_PredInv: return Stg_PredInv(o);
        case MXF_LsSize: return Stg_LsSize(o);
        case MXF_CacheOp: return Stg_CacheOp(o);
        case MXF_E: return Stg_E(o);
        case MXF_Imm24: return Stg_Imm24(o);
        }
        return MX_NO;
    case MXOP_Stl:
        switch (f) {
        case MXF_Dest: return Stl_Dest(o);
        case MXF_SrcA: return Stl_SrcA(o);
        case MXF_Pred: return Stl_Pred(o);
        case MXF_PredInv: return Stl_PredInv(o);
        case MXF_LsSize: return Stl_LsSize(o);
        case MXF_CacheOp: return Stl_CacheOp(o);
        case MXF_Imm24: return Stl_Imm24(o);
        }
        return MX_NO;
    case MXOP_Stp:
        switch (f) {
        case MXF_Wait: return Stp_Wait(o);
        case MXF_Imm8: return Stp_Imm8(o);
        }
        return MX_NO;
    case MXOP_Sts:
        switch (f) {
        case MXF_Dest: return Sts_Dest(o);
        case MXF_SrcA: return Sts_SrcA(o);
        case MXF_Pred: return Sts_Pred(o);
        case MXF_PredInv: return Sts_PredInv(o);
        case MXF_LsSize: return Sts_LsSize(o);
        case MXF_Imm24: return Sts_Imm24(o);
        }
        return MX_NO;
    case MXOP_Suatom:
        switch (f) {
        case MXF_Dest: return Suatom_Dest(o);
        case MXF_SrcA: return Suatom_SrcA(o);
        case MXF_SrcB: return Suatom_SrcB(o);
        case MXF_Pred: return Suatom_Pred(o);
        case MXF_PredInv: return Suatom_PredInv(o);
        case MXF_Size: return Suatom_Size(o);
        case MXF_Clamp: return Suatom_Clamp(o);
        case MXF_TidB: return Suatom_TidB(o);
        case MXF_Dim: return Suatom_Dim(o);
        case MXF_Op: return Suatom_Op(o);
        case MXF_Ba: return Suatom_Ba(o);
        }
        return MX_NO;
    case MXOP_SuatomB:
        switch (f) {
        case MXF_Dest: return SuatomB_Dest(o);
        case MXF_SrcA: return SuatomB_SrcA(o);
        case MXF_SrcB: return SuatomB_SrcB(o);
        case MXF_SrcC: return SuatomB_SrcC(o);
        case MXF_Pred: return SuatomB_Pred(o);
        case MXF_PredInv: return SuatomB_PredInv(o);
        case MXF_Clamp: return SuatomB_Clamp(o);
        case MXF_Size: return SuatomB_Size(o);
        case MXF_Dim: return SuatomB_Dim(o);
        case MXF_Op: return SuatomB_Op(o);
        case MXF_Ba: return SuatomB_Ba(o);
        }
        return MX_NO;
    case MXOP_SuatomB2:
        switch (f) {
        case MXF_Dest: return SuatomB2_Dest(o);
        case MXF_SrcA: return SuatomB2_SrcA(o);
        case MXF_SrcB: return SuatomB2_SrcB(o);
        case MXF_SrcC: return SuatomB2_SrcC(o);
        case MXF_Pred: return SuatomB2_Pred(o);
        case MXF_PredInv: return SuatomB2_PredInv(o);
        case MXF_DestPred: return SuatomB2_DestPred(o);
        case MXF_Clamp: return SuatomB2_Clamp(o);
        case MXF_Size: return SuatomB2_Size(o);
        case MXF_Dim: return SuatomB2_Dim(o);
        case MXF_Op: return SuatomB2_Op(o);
        case MXF_Ba: return SuatomB2_Ba(o);
        }
        return MX_NO;
    case MXOP_SuatomCas:
        switch (f) {
        case MXF_Dest: return SuatomCas_Dest(o);
        case MXF_SrcA: return SuatomCas_SrcA(o);
        case MXF_SrcB: return SuatomCas_SrcB(o);
        case MXF_Pred: return SuatomCas_Pred(o);
        case MXF_PredInv: return SuatomCas_PredInv(o);
        case MXF_Size: return SuatomCas_Size(o);
        case MXF_Clamp: return SuatomCas_Clamp(o);
        case MXF_TidB: return SuatomCas_TidB(o);
        case MXF_Dim: return SuatomCas_Dim(o);
        case MXF_DestPred: return SuatomCas_DestPred(o);
        case MXF_Ba: return SuatomCas_Ba(o);
        }
        return MX_NO;
    case MXOP_SuatomCasB:
        switch (f) {
        case MXF_Dest: return SuatomCasB_Dest(o);
        case MXF_SrcA: return SuatomCasB_SrcA(o);
        case MXF_SrcB: return SuatomCasB_SrcB(o);
        case MXF_SrcC: return SuatomCasB_SrcC(o);
        case MXF_Pred: return SuatomCasB_Pred(o);
        case MXF_PredInv: return SuatomCasB_PredInv(o);
        case MXF_Clamp: return SuatomCasB_Clamp(o);
        case MXF_Size: return SuatomCasB_Size(o);
        case MXF_Dim: return SuatomCasB_Dim(o);
        case MXF_DestPred: return SuatomCasB_DestPred(o);
        case MXF_Ba: return SuatomCasB_Ba(o);
        }
        return MX_NO;
    case MXOP_Suld:
        switch (f) {
        case MXF_Dest: return Suld_Dest(o);
        case MXF_SrcA: return Suld_SrcA(o);
        case MXF_Pred: return Suld_Pred(o);
        case MXF_PredInv: return Suld_PredInv(o);
        case MXF_Clamp: return Suld_Clamp(o);
        case MXF_TidB: return Suld_TidB(o);
        case MXF_Dim: return Suld_Dim(o);
        case MXF_DestPred2: return Suld_DestPred2(o);
        case MXF_CacheOp: return Suld_CacheOp(o);
        case MXF_Rgba: return Suld_Rgba(o);
        }
        return MX_NO;
    case MXOP_SuldB:
        switch (f) {
        case MXF_Dest: return SuldB_Dest(o);
        case MXF_SrcA: return SuldB_SrcA(o);
        case MXF_SrcC: return SuldB_SrcC(o);
        case MXF_Pred: return SuldB_Pred(o);
        case MXF_PredInv: return SuldB_PredInv(o);
        case MXF_Clamp: return SuldB_Clamp(o);
        case MXF_Dim: return SuldB_Dim(o);
        case MXF_DestPred2: return SuldB_DestPred2(o);
        case MXF_CacheOp: return SuldB_CacheOp(o);
        case MXF_Rgba: return SuldB_Rgba(o);
        }
        return MX_NO;
    case MXOP_SuldD:
        switch (f) {
        case MXF_Dest: return SuldD_Dest(o);
        case MXF_SrcA: return SuldD_SrcA(o);
        case MXF_Pred: return SuldD_Pred(o);
        case MXF_PredInv: return SuldD_PredInv(o);
        case MXF_Clamp: return SuldD_Clamp(o);
        case MXF_TidB: return SuldD_TidB(o);
        case MXF_Dim: return SuldD_Dim(o);
        case MXF_DestPred2: return SuldD_DestPred2(o);
        case MXF_CacheOp: return SuldD_CacheOp(o);
        case MXF_Ba: return SuldD_Ba(o);
        case MXF_Size: return SuldD_Size(o);
        }
        return MX_NO;
    case MXOP_SuldDB:
        switch (f) {
        case MXF_Dest: return SuldDB_Dest(o);
        case MXF_SrcA: return SuldDB_SrcA(o);
        case MXF_SrcC: return SuldDB_SrcC(o);
        case MXF_Pred: return SuldDB_Pred(o);
        case MXF_PredInv: return SuldDB_PredInv(o);
        case MXF_Clamp: return SuldDB_Clamp(o);
        case MXF_Dim: return SuldDB_Dim(o);
        case MXF_DestPred2: return SuldDB_DestPred2(o);
        case MXF_CacheOp: return SuldDB_CacheOp(o);
        case MXF_Ba: return SuldDB_Ba(o);
        case MXF_Size: return SuldDB_Size(o);
        }
        return MX_NO;
    case MXOP_Sured:
        switch (f) {
        case MXF_Dest: return Sured_Dest(o);
        case MXF_SrcA: return Sured_SrcA(o);
        case MXF_Pred: return Sured_Pred(o);
        case MXF_PredInv: return Sured_PredInv(o);
        case MXF_Clamp: return Sured_Clamp(o);
        case MXF_TidB: return Sured_TidB(o);
        case MXF_Dim: return Sured_Dim(o);
        case MXF_Op: return Sured_Op(o);
        case MXF_Ba: return Sured_Ba(o);
        case MXF_Size: return Sured_Size(o);
        }
        return MX_NO;
    case MXOP_SuredB:
        switch (f) {
        case MXF_Dest: return SuredB_Dest(o);
        case MXF_SrcA: return SuredB_SrcA(o);
        case MXF_SrcC: return SuredB_SrcC(o);
        case MXF_Pred: return SuredB_Pred(o);
        case MXF_PredInv: return SuredB_PredInv(o);
        case MXF_Clamp: return SuredB_Clamp(o);
        case MXF_Dim: return SuredB_Dim(o);
        case MXF_Op: return SuredB_Op(o);
        case MXF_Ba: return SuredB_Ba(o);
        case MXF_Size: return SuredB_Size(o);
        }
        return MX_NO;
    case MXOP_Sust:
        switch (f) {
        case MXF_Dest: return Sust_Dest(o);
        case MXF_SrcA: return Sust_SrcA(o);
        case MXF_Pred: return Sust_Pred(o);
        case MXF_PredInv: return Sust_PredInv(o);
        case MXF_Clamp: return Sust_Clamp(o);
        case MXF_TidB: return Sust_TidB(o);
        case MXF_Dim: return Sust_Dim(o);
        case MXF_CacheOp: return Sust_CacheOp(o);
        case MXF_Rgba: return Sust_Rgba(o);
        }
        return MX_NO;
    case MXOP_SustB:
        switch (f) {
        case MXF_Dest: return SustB_Dest(o);
        case MXF_SrcA: return SustB_SrcA(o);
        case MXF_SrcC: return SustB_SrcC(o);
        case MXF_Pred: return SustB_Pred(o);
        case MXF_PredInv: return SustB_PredInv(o);
        case MXF_Clamp: return SustB_Clamp(o);
        case MXF_Dim: return SustB_Dim(o);
        case MXF_CacheOp: return SustB_CacheOp(o);
        case MXF_Rgba: return SustB_Rgba(o);
        }
        return MX_NO;
    case MXOP_SustD:
        switch (f) {
        case MXF_Dest: return SustD_Dest(o);
        case MXF_SrcA: return SustD_SrcA(o);
        case MXF_Pred: return SustD_Pred(o);
        case MXF_PredInv: return SustD_PredInv(o);
        case MXF_Clamp: return SustD_Clamp(o);
        case MXF_TidB: return SustD_TidB(o);
        case MXF_Dim: return SustD_Dim(o);
        case MXF_CacheOp: return SustD_CacheOp(o);
        case MXF_Ba: return SustD_Ba(o);
        case MXF_Size: return SustD_Size(o);
        }
        return MX_NO;
    case MXOP_SustDB:
        switch (f) {
        case MXF_Dest: return SustDB_Dest(o);
        case MXF_SrcA: return SustDB_SrcA(o);
        case MXF_SrcC: return SustDB_SrcC(o);
        case MXF_Pred: return SustDB_Pred(o);
        case MXF_PredInv: return SustDB_PredInv(o);
        case MXF_Clamp: return SustDB_Clamp(o);
        case MXF_Dim: return SustDB_Dim(o);
        case MXF_CacheOp: return SustDB_CacheOp(o);
        case MXF_Ba: return SustDB_Ba(o);
        case MXF_Size: return SustDB_Size(o);
        }
        return MX_NO;
    case MXOP_Sync:
        switch (f) {
        case MXF_Pred: return Sync_Pred(o);
        case MXF_PredInv: return Sync_PredInv(o);
        case MXF_Ccc: return Sync_Ccc(o);
        }
        return MX_NO;
    case MXOP_Tex:
        switch (f) {
        case MXF_Dest: return Tex_Dest(o);
        case MXF_SrcA: return Tex_SrcA(o);
        case MXF_SrcB: return Tex_SrcB(o);
        case MXF_Pred: return Tex_Pred(o);
        case MXF_PredInv: return Tex_PredInv(o);
        case MXF_Lc: return Tex_Lc(o);
        case MXF_DestPred: return Tex_DestPred(o);
        case MXF_TidB: return Tex_TidB(o);
        case MXF_Lod: return Tex_Lod(o);
        case MXF_Aoffi: return Tex_Aoffi(o);
        case MXF_Dc: return Tex_Dc(o);
        case MXF_Ndv: return Tex_Ndv(o);
        case MXF_Dim: return Tex_Dim(o);
        case MXF_WMask: return Tex_WMask(o);
        case MXF_Nodep: return Tex_Nodep(o);
        }
        return MX_NO;
    case MXOP_TexB:
        switch (f) {
        case MXF_Dest: return TexB_Dest(o);
        case MXF_SrcA: return TexB_SrcA(o);
        case MXF_SrcB: return TexB_SrcB(o);
        case MXF_Pred: return TexB_Pred(o);
        case MXF_PredInv: return TexB_PredInv(o);
        case MXF_Lcb: return TexB_Lcb(o);
        case MXF_DestPred: return TexB_DestPred(o);
        case MXF_Lodb: return TexB_Lodb(o);
        case MXF_Aoffib: return TexB_Aoffib(o);
        case MXF_Dc: return TexB_Dc(o);
        case MXF_Ndv: return TexB_Ndv(o);
        case MXF_Dim: return TexB_Dim(o);
        case MXF_WMask: return TexB_WMask(o);
        case MXF_Nodep: return TexB_Nodep(o);
        }
        return MX_NO;
    case MXOP_Texs:
        switch (f) {
        case MXF_Dest: return Texs_Dest(o);
        case MXF_SrcA: return Texs_SrcA(o);
        case MXF_SrcB: return Texs_SrcB(o);
        case MXF_Pred: return Texs_Pred(o);
        case MXF_PredInv: return Texs_PredInv(o);
        case MXF_TidB: return Texs_TidB(o);
        case MXF_Target: return Texs_Target(o);
        case MXF_WMask: return Texs_WMask(o);
        case MXF_Nodep: return Texs_Nodep(o);
        case MXF_Dest2: return Texs_Dest2(o);
        }
        return MX_NO;
    case MXOP_TexsF16:
        switch (f) {
        case MXF_Dest: return TexsF16_Dest(o);
        case MXF_SrcA: return TexsF16_SrcA(o);
        case MXF_SrcB: return TexsF16_SrcB(o);
        case MXF_Pred: return TexsF16_Pred(o);
        case MXF_PredInv: return TexsF16_PredInv(o);
        case MXF_TidB: return TexsF16_TidB(o);
        case MXF_Target: return TexsF16_Target(o);
        case MXF_WMask: return TexsF16_WMask(o);
        case MXF_Nodep: return TexsF16_Nodep(o);
        case MXF_Dest2: return TexsF16_Dest2(o);
        }
        return MX_NO;
    case MXOP_Tld:
        switch (f) {
        case MXF_Dest: return Tld_Dest(o);
        case MXF_SrcA: return Tld_SrcA(o);
        case MXF_SrcB: return Tld_SrcB(o);
        case MXF_Pred: return Tld_Pred(o);
        case MXF_PredInv: return Tld_PredInv(o);
        case MXF_TidB: return Tld_TidB(o);
        case MXF_WMask: return Tld_WMask(o);
        case MXF_Lod: return Tld_Lod(o);
        case MXF_Toff: return Tld_Toff(o);
        case MXF_Ms: return Tld_Ms(o);
        case MXF_Cl: return Tld_Cl(o);
        case MXF_Nodep: return Tld_Nodep(o);
        case MXF_DestPred: return Tld_DestPred(o);
        case MXF_Dim: return Tld_Dim(o);
        }
        return MX_NO;
    case MXOP_Tld4:
        switch (f) {
        case MXF_Dest: return Tld4_Dest(o);
        case MXF_SrcA: return Tld4_SrcA(o);
        case MXF_SrcB: return Tld4_SrcB(o);
        case MXF_Pred: return Tld4_Pred(o);
        case MXF_PredInv: return Tld4_PredInv(o);
        case MXF_Lc: return Tld4_Lc(o);
        case MXF_DestPred: return Tld4_DestPred(o);
        case MXF_TidB: return Tld4_TidB(o);
        case MXF_TexComp: return Tld4_TexComp(o);
        case MXF_Toff: return Tld4_Toff(o);
        case MXF_Dc: return Tld4_Dc(o);
        case MXF_Ndv: return Tld4_Ndv(o);
        case MXF_Dim: return Tld4_Dim(o);
        case MXF_WMask: return Tld4_WMask(o);
        case MXF_Nodep: return Tld4_Nodep(o);
        }
        return MX_NO;
    case MXOP_Tld4B:
        switch (f) {
        case MXF_Dest: return Tld4B_Dest(o);
        case MXF_SrcA: return Tld4B_SrcA(o);
        case MXF_SrcB: return Tld4B_SrcB(o);
        case MXF_Pred: return Tld4B_Pred(o);
        case MXF_PredInv: return Tld4B_PredInv(o);
        case MXF_Lc: return Tld4B_Lc(o);
        case MXF_DestPred: return Tld4B_DestPred(o);
        case MXF_TexComp: return Tld4B_TexComp(o);
        case MXF_Toff: return Tld4B_Toff(o);
        case MXF_Dc: return Tld4B_Dc(o);
        case MXF_Ndv: return Tld4B_Ndv(o);
        case MXF_Dim: return Tld4B_Dim(o);
        case MXF_WMask: return Tld4B_WMask(o);
        case MXF_Nodep: return Tld4B_Nodep(o);
        }
        return MX_NO;
    case MXOP_Tld4s:
        switch (f) {
        case MXF_Dest: return Tld4s_Dest(o);
        case MXF_SrcA: return Tld4s_SrcA(o);
        case MXF_SrcB: return Tld4s_SrcB(o);
        case MXF_Pred: return Tld4s_Pred(o);
        case MXF_PredInv: return Tld4s_PredInv(o);
        case MXF_TidB: return Tld4s_TidB(o);
        case MXF_TexComp: return Tld4s_TexComp(o);
        case MXF_Aoffi: return Tld4s_Aoffi(o);
        case MXF_Dc: return Tld4s_Dc(o);
        case MXF_Nodep: return Tld4s_Nodep(o);
        case MXF_Dest2: return Tld4s_Dest2(o);
        }
        return MX_NO;
    case MXOP_Tld4sF16:
        switch (f) {
        case MXF_Dest: return Tld4sF16_Dest(o);
        case MXF_SrcA: return Tld4sF16_SrcA(o);
        case MXF_SrcB: return Tld4sF16_SrcB(o);
        case MXF_Pred: return Tld4sF16_Pred(o);
        case MXF_PredInv: return Tld4sF16_PredInv(o);
        case MXF_TidB: return Tld4sF16_TidB(o);
        case MXF_TexComp: return Tld4sF16_TexComp(o);
        case MXF_Aoffi: return Tld4sF16_Aoffi(o);
        case MXF_Dc: return Tld4sF16_Dc(o);
        case MXF_Nodep: return Tld4sF16_Nodep(o);
        case MXF_Dest2: return Tld4sF16_Dest2(o);
        }
        return MX_NO;
    case MXOP_TldB:
        switch (f) {
        case MXF_Dest: return TldB_Dest(o);
        case MXF_SrcA: return TldB_SrcA(o);
        case MXF_SrcB: return TldB_SrcB(o);
        case MXF_Pred: return TldB_Pred(o);
        case MXF_PredInv: return TldB_PredInv(o);
        case MXF_WMask: return TldB_WMask(o);
        case MXF_Lod: return TldB_Lod(o);
        case MXF_Toff: return TldB_Toff(o);
        case MXF_Ms: return TldB_Ms(o);
        case MXF_Cl: return TldB_Cl(o);
        case MXF_Nodep: return TldB_Nodep(o);
        case MXF_DestPred: return TldB_DestPred(o);
        case MXF_Dim: return TldB_Dim(o);
        }
        return MX_NO;
    case MXOP_Tlds:
        switch (f) {
        case MXF_Dest: return Tlds_Dest(o);
        case MXF_SrcA: return Tlds_SrcA(o);
        case MXF_SrcB: return Tlds_SrcB(o);
        case MXF_Pred: return Tlds_Pred(o);
        case MXF_PredInv: return Tlds_PredInv(o);
        case MXF_TidB: return Tlds_TidB(o);
        case MXF_Target: return Tlds_Target(o);
        case MXF_WMask: return Tlds_WMask(o);
        case MXF_Nodep: return Tlds_Nodep(o);
        case MXF_Dest2: return Tlds_Dest2(o);
        }
        return MX_NO;
    case MXOP_TldsF16:
        switch (f) {
        case MXF_Dest: return TldsF16_Dest(o);
        case MXF_SrcA: return TldsF16_SrcA(o);
        case MXF_SrcB: return TldsF16_SrcB(o);
        case MXF_Pred: return TldsF16_Pred(o);
        case MXF_PredInv: return TldsF16_PredInv(o);
        case MXF_TidB: return TldsF16_TidB(o);
        case MXF_Target: return TldsF16_Target(o);
        case MXF_WMask: return TldsF16_WMask(o);
        case MXF_Nodep: return TldsF16_Nodep(o);
        case MXF_Dest2: return TldsF16_Dest2(o);
        }
        return MX_NO;
    case MXOP_Tmml:
        switch (f) {
        case MXF_Dest: return Tmml_Dest(o);
        case MXF_SrcA: return Tmml_SrcA(o);
        case MXF_SrcB: return Tmml_SrcB(o);
        case MXF_Pred: return Tmml_Pred(o);
        case MXF_PredInv: return Tmml_PredInv(o);
        case MXF_Nodep: return Tmml_Nodep(o);
        case MXF_TidB: return Tmml_TidB(o);
        case MXF_Ndv: return Tmml_Ndv(o);
        case MXF_WMask: return Tmml_WMask(o);
        case MXF_Dim: return Tmml_Dim(o);
        }
        return MX_NO;
    case MXOP_TmmlB:
        switch (f) {
        case MXF_Dest: return TmmlB_Dest(o);
        case MXF_SrcA: return TmmlB_SrcA(o);
        case MXF_SrcB: return TmmlB_SrcB(o);
        case MXF_Pred: return TmmlB_Pred(o);
        case MXF_PredInv: return TmmlB_PredInv(o);
        case MXF_Nodep: return TmmlB_Nodep(o);
        case MXF_Ndv: return TmmlB_Ndv(o);
        case MXF_WMask: return TmmlB_WMask(o);
        case MXF_Dim: return TmmlB_Dim(o);
        }
        return MX_NO;
    case MXOP_Txa:
        switch (f) {
        case MXF_Dest: return Txa_Dest(o);
        case MXF_SrcA: return Txa_SrcA(o);
        case MXF_Pred: return Txa_Pred(o);
        case MXF_PredInv: return Txa_PredInv(o);
        case MXF_Nodep: return Txa_Nodep(o);
        case MXF_TidB: return Txa_TidB(o);
        case MXF_Ndv: return Txa_Ndv(o);
        case MXF_WMask: return Txa_WMask(o);
        }
        return MX_NO;
    case MXOP_Txd:
        switch (f) {
        case MXF_Dest: return Txd_Dest(o);
        case MXF_SrcA: return Txd_SrcA(o);
        case MXF_SrcB: return Txd_SrcB(o);
        case MXF_Pred: return Txd_Pred(o);
        case MXF_PredInv: return Txd_PredInv(o);
        case MXF_DestPred: return Txd_DestPred(o);
        case MXF_Lc: return Txd_Lc(o);
        case MXF_Nodep: return Txd_Nodep(o);
        case MXF_TidB: return Txd_TidB(o);
        case MXF_Toff: return Txd_Toff(o);
        case MXF_WMask: return Txd_WMask(o);
        case MXF_Dim: return Txd_Dim(o);
        }
        return MX_NO;
    case MXOP_TxdB:
        switch (f) {
        case MXF_Dest: return TxdB_Dest(o);
        case MXF_SrcA: return TxdB_SrcA(o);
        case MXF_SrcB: return TxdB_SrcB(o);
        case MXF_Pred: return TxdB_Pred(o);
        case MXF_PredInv: return TxdB_PredInv(o);
        case MXF_DestPred: return TxdB_DestPred(o);
        case MXF_Lc: return TxdB_Lc(o);
        case MXF_Nodep: return TxdB_Nodep(o);
        case MXF_Toff: return TxdB_Toff(o);
        case MXF_WMask: return TxdB_WMask(o);
        case MXF_Dim: return TxdB_Dim(o);
        }
        return MX_NO;
    case MXOP_Txq:
        switch (f) {
        case MXF_Dest: return Txq_Dest(o);
        case MXF_SrcA: return Txq_SrcA(o);
        case MXF_Pred: return Txq_Pred(o);
        case MXF_PredInv: return Txq_PredInv(o);
        case MXF_Nodep: return Txq_Nodep(o);
        case MXF_TidB: return Txq_TidB(o);
        case MXF_WMask: return Txq_WMask(o);
        case MXF_TexQuery: return Txq_TexQuery(o);
        }
        return MX_NO;
    case MXOP_TxqB:
        switch (f) {
        case MXF_Dest: return TxqB_Dest(o);
        case MXF_SrcA: return TxqB_SrcA(o);
        case MXF_Pred: return TxqB_Pred(o);
        case MXF_PredInv: return TxqB_PredInv(o);
        case MXF_Nodep: return TxqB_Nodep(o);
        case MXF_WMask: return TxqB_WMask(o);
        case MXF_TexQuery: return TxqB_TexQuery(o);
        }
        return MX_NO;
    case MXOP_Vabsdiff:
        switch (f) {
        case MXF_Dest: return Vabsdiff_Dest(o);
        case MXF_SrcA: return Vabsdiff_SrcA(o);
        case MXF_SrcB: return Vabsdiff_SrcB(o);
        case MXF_SrcC: return Vabsdiff_SrcC(o);
        case MXF_Pred: return Vabsdiff_Pred(o);
        case MXF_PredInv: return Vabsdiff_PredInv(o);
        case MXF_WriteCC: return Vabsdiff_WriteCC(o);
        case MXF_DFormat: return Vabsdiff_DFormat(o);
        case MXF_ASelect: return Vabsdiff_ASelect(o);
        case MXF_BSelect: return Vabsdiff_BSelect(o);
        case MXF_Sat: return Vabsdiff_Sat(o);
        case MXF_VideoOp: return Vabsdiff_VideoOp(o);
        case MXF_BVideo: return Vabsdiff_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vabsdiff4:
        switch (f) {
        case MXF_Dest: return Vabsdiff4_Dest(o);
        case MXF_SrcA: return Vabsdiff4_SrcA(o);
        case MXF_SrcB: return Vabsdiff4_SrcB(o);
        case MXF_SrcC: return Vabsdiff4_SrcC(o);
        case MXF_Pred: return Vabsdiff4_Pred(o);
        case MXF_PredInv: return Vabsdiff4_PredInv(o);
        case MXF_WriteCC: return Vabsdiff4_WriteCC(o);
        case MXF_VRed: return Vabsdiff4_VRed(o);
        case MXF_LaneMask4: return Vabsdiff4_LaneMask4(o);
        case MXF_Sat: return Vabsdiff4_Sat(o);
        case MXF_SrcBFmt: return Vabsdiff4_SrcBFmt(o);
        case MXF_SrcAFmt: return Vabsdiff4_SrcAFmt(o);
        case MXF_DFormat: return Vabsdiff4_DFormat(o);
        case MXF_Asel4: return Vabsdiff4_Asel4(o);
        case MXF_Bsel4: return Vabsdiff4_Bsel4(o);
        }
        return MX_NO;
    case MXOP_Vadd:
        switch (f) {
        case MXF_Dest: return Vadd_Dest(o);
        case MXF_SrcA: return Vadd_SrcA(o);
        case MXF_SrcB: return Vadd_SrcB(o);
        case MXF_SrcC: return Vadd_SrcC(o);
        case MXF_Pred: return Vadd_Pred(o);
        case MXF_PredInv: return Vadd_PredInv(o);
        case MXF_Imm16: return Vadd_Imm16(o);
        case MXF_WriteCC: return Vadd_WriteCC(o);
        case MXF_AvgMode: return Vadd_AvgMode(o);
        case MXF_DFormat: return Vadd_DFormat(o);
        case MXF_ASelect: return Vadd_ASelect(o);
        case MXF_BSelect: return Vadd_BSelect(o);
        case MXF_Sat: return Vadd_Sat(o);
        case MXF_VideoOp: return Vadd_VideoOp(o);
        case MXF_BVideo: return Vadd_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vmad:
        switch (f) {
        case MXF_Dest: return Vmad_Dest(o);
        case MXF_SrcA: return Vmad_SrcA(o);
        case MXF_SrcB: return Vmad_SrcB(o);
        case MXF_SrcC: return Vmad_SrcC(o);
        case MXF_Pred: return Vmad_Pred(o);
        case MXF_PredInv: return Vmad_PredInv(o);
        case MXF_Imm16: return Vmad_Imm16(o);
        case MXF_WriteCC: return Vmad_WriteCC(o);
        case MXF_ASelect: return Vmad_ASelect(o);
        case MXF_BSelect: return Vmad_BSelect(o);
        case MXF_Sat: return Vmad_Sat(o);
        case MXF_AvgMode: return Vmad_AvgMode(o);
        case MXF_VideoScale: return Vmad_VideoScale(o);
        case MXF_BVideo: return Vmad_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vmnmx:
        switch (f) {
        case MXF_Dest: return Vmnmx_Dest(o);
        case MXF_SrcA: return Vmnmx_SrcA(o);
        case MXF_SrcB: return Vmnmx_SrcB(o);
        case MXF_SrcC: return Vmnmx_SrcC(o);
        case MXF_Pred: return Vmnmx_Pred(o);
        case MXF_PredInv: return Vmnmx_PredInv(o);
        case MXF_Imm16: return Vmnmx_Imm16(o);
        case MXF_WriteCC: return Vmnmx_WriteCC(o);
        case MXF_DFormat: return Vmnmx_DFormat(o);
        case MXF_ASelect: return Vmnmx_ASelect(o);
        case MXF_BSelect: return Vmnmx_BSelect(o);
        case MXF_Sat: return Vmnmx_Sat(o);
        case MXF_VideoOp: return Vmnmx_VideoOp(o);
        case MXF_Mn: return Vmnmx_Mn(o);
        case MXF_BVideo: return Vmnmx_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vote:
        switch (f) {
        case MXF_Dest: return Vote_Dest(o);
        case MXF_Pred: return Vote_Pred(o);
        case MXF_PredInv: return Vote_PredInv(o);
        case MXF_SrcPred: return Vote_SrcPred(o);
        case MXF_SrcPredInv: return Vote_SrcPredInv(o);
        case MXF_VoteMode: return Vote_VoteMode(o);
        case MXF_VpDest: return Vote_VpDest(o);
        }
        return MX_NO;
    case MXOP_Votevtg:
        switch (f) {
        case MXF_Pred: return Votevtg_Pred(o);
        case MXF_PredInv: return Votevtg_PredInv(o);
        case MXF_VoteMode: return Votevtg_VoteMode(o);
        case MXF_Imm28: return Votevtg_Imm28(o);
        }
        return MX_NO;
    case MXOP_Vset:
        switch (f) {
        case MXF_Dest: return Vset_Dest(o);
        case MXF_SrcA: return Vset_SrcA(o);
        case MXF_SrcB: return Vset_SrcB(o);
        case MXF_SrcC: return Vset_SrcC(o);
        case MXF_Pred: return Vset_Pred(o);
        case MXF_PredInv: return Vset_PredInv(o);
        case MXF_WriteCC: return Vset_WriteCC(o);
        case MXF_VComp: return Vset_VComp(o);
        case MXF_ASelect: return Vset_ASelect(o);
        case MXF_BSelect: return Vset_BSelect(o);
        case MXF_VideoOp: return Vset_VideoOp(o);
        case MXF_BVideo: return Vset_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vsetp:
        switch (f) {
        case MXF_SrcA: return Vsetp_SrcA(o);
        case MXF_SrcB: return Vsetp_SrcB(o);
        case MXF_Pred: return Vsetp_Pred(o);
        case MXF_PredInv: return Vsetp_PredInv(o);
        case MXF_Imm16: return Vsetp_Imm16(o);
        case MXF_ASelect: return Vsetp_ASelect(o);
        case MXF_BSelect: return Vsetp_BSelect(o);
        case MXF_VComp: return Vsetp_VComp(o);
        case MXF_BoolOp: return Vsetp_BoolOp(o);
        case MXF_SrcPred: return Vsetp_SrcPred(o);
        case MXF_SrcPredInv: return Vsetp_SrcPredInv(o);
        case MXF_DestPred: return Vsetp_DestPred(o);
        case MXF_DestPredInv: return Vsetp_DestPredInv(o);
        case MXF_BVideo: return Vsetp_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vshl:
        switch (f) {
        case MXF_Dest: return Vshl_Dest(o);
        case MXF_SrcA: return Vshl_SrcA(o);
        case MXF_SrcB: return Vshl_SrcB(o);
        case MXF_SrcC: return Vshl_SrcC(o);
        case MXF_Pred: return Vshl_Pred(o);
        case MXF_PredInv: return Vshl_PredInv(o);
        case MXF_WriteCC: return Vshl_WriteCC(o);
        case MXF_Mv: return Vshl_Mv(o);
        case MXF_DFormat: return Vshl_DFormat(o);
        case MXF_ASelect: return Vshl_ASelect(o);
        case MXF_BSelect: return Vshl_BSelect(o);
        case MXF_Sat: return Vshl_Sat(o);
        case MXF_VideoOp: return Vshl_VideoOp(o);
        case MXF_BVideo: return Vshl_BVideo(o);
        }
        return MX_NO;
    case MXOP_Vshr:
        switch (f) {
        case MXF_Dest: return Vshr_Dest(o);
        case MXF_SrcA: return Vshr_SrcA(o);
        case MXF_SrcB: return Vshr_SrcB(o);
        case MXF_SrcC: return Vshr_SrcC(o);
        case MXF_Pred: return Vshr_Pred(o);
        case MXF_PredInv: return Vshr_PredInv(o);
        case MXF_WriteCC: return Vshr_WriteCC(o);
        case MXF_Mv: return Vshr_Mv(o);
        case MXF_DFormat: return Vshr_DFormat(o);
        case MXF_ASelect: return Vshr_ASelect(o);
        case MXF_BSelect: return Vshr_BSelect(o);
        case MXF_Sat: return Vshr_Sat(o);
        case MXF_VideoOp: return Vshr_VideoOp(o);
        case MXF_BVideo: return Vshr_BVideo(o);
        }
        return MX_NO;
    case MXOP_XmadC:
        switch (f) {
        case MXF_Dest: return XmadC_Dest(o);
        case MXF_SrcA: return XmadC_SrcA(o);
        case MXF_CbufSlot: return XmadC_CbufSlot(o);
        case MXF_CbufOffset: return XmadC_CbufOffset(o);
        case MXF_SrcC: return XmadC_SrcC(o);
        case MXF_Pred: return XmadC_Pred(o);
        case MXF_PredInv: return XmadC_PredInv(o);
        case MXF_WriteCC: return XmadC_WriteCC(o);
        case MXF_Mrg: return XmadC_Mrg(o);
        case MXF_Psl: return XmadC_Psl(o);
        case MXF_X: return XmadC_X(o);
        case MXF_HiloA: return XmadC_HiloA(o);
        case MXF_HiloB: return XmadC_HiloB(o);
        case MXF_XmadCop: return XmadC_XmadCop(o);
        case MXF_BSigned: return XmadC_BSigned(o);
        case MXF_ASigned: return XmadC_ASigned(o);
        }
        return MX_NO;
    case MXOP_XmadI:
        switch (f) {
        case MXF_Dest: return XmadI_Dest(o);
        case MXF_SrcA: return XmadI_SrcA(o);
        case MXF_SrcC: return XmadI_SrcC(o);
        case MXF_Pred: return XmadI_Pred(o);
        case MXF_PredInv: return XmadI_PredInv(o);
        case MXF_WriteCC: return XmadI_WriteCC(o);
        case MXF_Imm16: return XmadI_Imm16(o);
        case MXF_HiloA: return XmadI_HiloA(o);
        case MXF_XmadCop: return XmadI_XmadCop(o);
        case MXF_BSigned: return XmadI_BSigned(o);
        case MXF_ASigned: return XmadI_ASigned(o);
        case MXF_X: return XmadI_X(o);
        case MXF_Mrg: return XmadI_Mrg(o);
        case MXF_Psl: return XmadI_Psl(o);
        }
        return MX_NO;
    case MXOP_XmadR:
        switch (f) {
        case MXF_Dest: return XmadR_Dest(o);
        case MXF_SrcA: return XmadR_SrcA(o);
        case MXF_SrcB: return XmadR_SrcB(o);
        case MXF_SrcC: return XmadR_SrcC(o);
        case MXF_Pred: return XmadR_Pred(o);
        case MXF_PredInv: return XmadR_PredInv(o);
        case MXF_WriteCC: return XmadR_WriteCC(o);
        case MXF_HiloA: return XmadR_HiloA(o);
        case MXF_XmadCop: return XmadR_XmadCop(o);
        case MXF_BSigned: return XmadR_BSigned(o);
        case MXF_ASigned: return XmadR_ASigned(o);
        case MXF_X: return XmadR_X(o);
        case MXF_Mrg: return XmadR_Mrg(o);
        case MXF_Psl: return XmadR_Psl(o);
        case MXF_HiloB: return XmadR_HiloB(o);
        }
        return MX_NO;
    case MXOP_XmadRc:
        switch (f) {
        case MXF_Dest: return XmadRc_Dest(o);
        case MXF_SrcA: return XmadRc_SrcA(o);
        case MXF_SrcC: return XmadRc_SrcC(o);
        case MXF_CbufSlot: return XmadRc_CbufSlot(o);
        case MXF_CbufOffset: return XmadRc_CbufOffset(o);
        case MXF_Pred: return XmadRc_Pred(o);
        case MXF_PredInv: return XmadRc_PredInv(o);
        case MXF_WriteCC: return XmadRc_WriteCC(o);
        case MXF_X: return XmadRc_X(o);
        case MXF_HiloA: return XmadRc_HiloA(o);
        case MXF_HiloB: return XmadRc_HiloB(o);
        case MXF_XmadCop: return XmadRc_XmadCop(o);
        case MXF_BSigned: return XmadRc_BSigned(o);
        case MXF_ASigned: return XmadRc_ASigned(o);
        }
        return MX_NO;
    }
    return MX_NO;
}
