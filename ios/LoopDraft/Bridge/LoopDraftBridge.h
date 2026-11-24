#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface LoopDraftBridge : NSObject

+ (instancetype)shared;

- (BOOL)startWithFilesDir:(NSString *)dir;
- (void)stop;
- (NSString *)audioInfo;
- (void)saveIfDirty;
- (BOOL)poll;

- (void)playScene:(int)scene;
- (void)stopTransport;
- (void)queueScene:(int)scene;
- (void)setSongMode:(BOOL)on;
- (void)setRecording:(BOOL)on;
- (void)triggerPad:(int)pad vel:(int)vel;
- (void)noteOnSynth:(int)synth degree:(int)degree vel:(int)vel NS_SWIFT_NAME(noteOnSynth(_:degree:vel:));
- (void)noteOffSynth:(int)synth degree:(int)degree NS_SWIFT_NAME(noteOffSynth(_:degree:));

- (BOOL)isPlaying;
- (int)currentStep;
- (int)currentScene;
- (int)queuedScene;
- (int)chainPos;
- (float)peakForChannel:(int)ch;

- (void)beginEdit;
- (BOOL)undo;
- (BOOL)redo;
- (BOOL)canUndo;
- (BOOL)canRedo;

- (float)getParam:(int)paramId index:(int)index;
- (void)setParam:(int)paramId index:(int)index value:(float)value;

- (int)toggleStepScene:(int)scene pad:(int)pad step:(int)step accent:(BOOL)accent;
- (void)setDrumStepScene:(int)scene pad:(int)pad step:(int)step value:(int)value;
- (NSData *)drumGridForScene:(int)scene;
- (BOOL)toggleNoteScene:(int)scene synth:(int)synth step:(int)step degree:(int)degree accent:(BOOL)accent;
- (NSArray<NSNumber *> *)notesForScene:(int)scene synth:(int)synth;
- (int)degreeMidiForSynth:(int)synth degree:(int)degree;

- (void)setDrumEngine:(int)engine;
- (void)applyJungleTuningPitch:(float)pitch tight:(float)tight crunch:(float)crunch;
- (BOOL)loadJungleBreakWav:(NSString *)path;
- (void)resampleSceneToJungle:(int)scene;

- (int)sceneBars:(int)scene;
- (void)setSceneBars:(int)scene bars:(int)bars;
- (void)copySceneFrom:(int)from to:(int)to;
- (void)clearScene:(int)scene;
- (NSArray<NSNumber *> *)chain;
- (void)chainAdd:(int)scene;
- (void)chainRemoveAt:(int)pos;
- (void)resetToDemo;

- (void)setChainAutoPos:(int)pos track:(int)track lane:(int)lane paramId:(int)paramId startVal:(float)startVal endVal:(float)endVal;
- (NSArray<NSNumber *> *)chainAutoPos:(int)pos track:(int)track;
- (void)setFxSlotTrack:(int)track slot:(int)slot type:(int)type amount:(float)amount rate:(float)rate bypass:(BOOL)bypass;
- (NSArray<NSNumber *> *)fxSlotTrack:(int)track slot:(int)slot;
- (void)applyInstrumentSynth:(int)synth preset:(int)preset;
- (void)setActiveSynths:(int)count;

/// What a control is set to in a new project (double-tap reset).
- (float)defaultParam:(int)paramId index:(int)index NS_SWIFT_NAME(defaultParam(_:index:));

/// A preset, drum kit or groove file (docs/CONTENT_FORMAT.md). Returns "" when it was loaded, otherwise what is wrong with it.
- (NSString *)importContent:(NSString *)json scene:(int)scene synth:(int)synth tempoKey:(BOOL)tempoKey NS_SWIFT_NAME(importContent(_:scene:synth:tempoKey:));
/// [kind, name, style] of such a file, or an empty array when it is not valid.
- (NSArray<NSString *> *)contentInfo:(NSString *)json NS_SWIFT_NAME(contentInfo(_:));

- (BOOL)saveProjectCopy:(NSString *)path NS_SWIFT_NAME(saveProjectCopy(_:));
- (BOOL)openProject:(NSString *)path NS_SWIFT_NAME(openProject(_:));

/// Renders on a background queue from a copy of the project; `completion` runs on the main queue.
/// stem: -1 the full mix, 0 drums, 1 bass, 2 lead (WAV only).
- (void)exportTo:(NSString *)path midi:(BOOL)midi song:(BOOL)song scene:(int)scene stem:(int)stem completion:(void (^)(BOOL ok))completion NS_SWIFT_NAME(export(to:midi:song:scene:stem:completion:));
/// What is in the project as a library file. kind: 0 instrument (of `synth`), 1 drum kit, 2 groove (of `scene`).
- (NSString *)exportContent:(int)kind scene:(int)scene synth:(int)synth name:(NSString *)name NS_SWIFT_NAME(exportContent(kind:scene:synth:name:));

- (void)loadStartPreset;
- (void)writeBreakPattern:(int)scene NS_SWIFT_NAME(writeBreakPattern(_:));
/// Outline of a pad's sound for the display: `count` peak values 0..1.
- (NSArray<NSNumber *> *)padWave:(int)pad count:(int)count NS_SWIFT_NAME(padWave(_:count:));
- (BOOL)hasOwnBreak;
/// Like resampleSceneToJungle, but says whether there was something to bounce.
- (BOOL)bounceScene:(int)scene NS_SWIFT_NAME(bounceScene(_:));
- (void)applyDrumSynthTemplate:(int)pad tmpl:(int)tmpl NS_SWIFT_NAME(applyDrumSynthTemplate(_:template:));
- (void)clearCustomPad:(int)pad NS_SWIFT_NAME(clearCustomPad(_:));
- (BOOL)padIsSynth:(int)pad NS_SWIFT_NAME(padIsSynth(_:));

- (BOOL)toggleNote:(int)scene synth:(int)synth step:(int)step degree:(int)degree accent:(BOOL)accent slide:(BOOL)slide NS_SWIFT_NAME(toggleNote(scene:synth:step:degree:accent:slide:));
/// The notes that glide in, as a flat list: step, degree, step, degree, ...
- (NSArray<NSNumber *> *)slideNotes:(int)scene synth:(int)synth NS_SWIFT_NAME(slideNotes(scene:synth:));
/// Like setChainAuto, but without a new undo step for every movement of a control.
- (void)setChainAutoLive:(int)pos track:(int)track lane:(int)lane paramId:(int)paramId startVal:(float)startVal endVal:(float)endVal NS_SWIFT_NAME(setChainAutoLive(pos:track:lane:paramId:start:end:));

/// 0 fast, 1 balanced, 2 safe: the audio buffer the app asks the system for.
- (void)setAudioMode:(int)mode NS_SWIFT_NAME(setAudioMode(_:));

@end

NS_ASSUME_NONNULL_END
