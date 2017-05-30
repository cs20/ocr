# Regression infrastructure uses basevalues supplied by app developer
# to normalize trend plots
# Add entry for each test -> 'test_name':value_to_normalize_against
# test_name should be same as run job name
# If you don't desire normalization , make an entry and pass
# 1 as value. If you are adding a new app , find avg. runtime by
# running app locally and use it.

perftest_baseval={
    # Based on perf mode parameters as defined in the task 'ocr-build-x86-perfs'
    'edtCreateFinishSync':2148107.159,
    'edtCreateLatchSync':2424469.355,
    'edtTemplate0Create': 5961044.01,
    'edtTemplate0Destroy':8468996.547,
    'event0ChannelCreate':5068555.396,
    'event0ChannelDestroy':7731840.405,
    'event0CountedCreate':5382682.112,
    'event0CountedDestroy':8027072.784,
    'event0LatchCreate':5293489.002,
    'event0LatchDestroy':8037983.187,
    'event0OnceCreate':5225439.383,
    'event0OnceDestroy' :7740270.386,
    'event0StickyCreate':5189435.916,
    'event0StickyDestroy':7448754.635,
    'event1OnceFanOutEdtAddDep':4365307.741,
    'event1OnceFanOutEdtSatisfy':2861090.117,
    'event1StickyFanOutEdtAddDep':3591335.574,
    'event1StickyFanOutEdtSatisfy':3307069.898,
    'event2LatchFanOutLatchAddDep':8694114.629,
    'event2LatchFanOutLatchSatisfy':5366450.834,
    'event2OnceFanOutOnceAddDep':8975390.679,
    'event2OnceFanOutOnceSatisfy':3835140.08,
    'event2StickyFanOutStickyAddDep':6658578.063,
    'event2StickyFanOutStickySatisfy':6379660.944,
}
