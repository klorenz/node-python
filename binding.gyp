{
  "targets": [
    {
      "target_name": "binding",
      "sources": [
        "src/binding.cc",
        "src/utils.cc"
      ],
      "conditions": [
        ['OS=="mac"', {
          "include_dirs": [
            "$(SDKROOT)/System/Library/Frameworks/Python.framework/Versions/Current/Headers"
          ],
          "link_settings": {
            "libraries": [
              "$(SDKROOT)/System/Library/Frameworks/Python.framework"
            ]
          }
        }]
      ]
    }
  ]
}
